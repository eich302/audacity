/*
* Audacity: A Digital Audio Editor
*/
#include "ffmpegvideodecoder.h"

#include <algorithm>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

using namespace au::video;

// AVFrame::pkt_duration is deprecated (FFmpeg 7 / libavutil 59 removes it), AVFrame::duration exists since libavutil 58
#if LIBAVUTIL_VERSION_MAJOR >= 58
#define AU_FRAME_DURATION(f) ((f)->duration)
#else
#define AU_FRAME_DURATION(f) ((f)->pkt_duration)
#endif

namespace {
constexpr int MAX_SIDE = 640;
constexpr int MAX_FRAMES_AHEAD_NO_SEEK = 8;
}

void FfmpegVideoDecoder::FormatDeleter::operator()(AVFormatContext* p) const { avformat_close_input(&p); }
void FfmpegVideoDecoder::CodecDeleter::operator()(AVCodecContext* p) const { avcodec_free_context(&p); }
void FfmpegVideoDecoder::FrameDeleter::operator()(AVFrame* p) const { av_frame_free(&p); }
void FfmpegVideoDecoder::PacketDeleter::operator()(AVPacket* p) const { av_packet_free(&p); }
void FfmpegVideoDecoder::SwsDeleter::operator()(SwsContext* p) const { sws_freeContext(p); }

FfmpegVideoDecoder::FfmpegVideoDecoder() = default;

FfmpegVideoDecoder::~FfmpegVideoDecoder()
{
    close();
}

bool FfmpegVideoDecoder::open(const std::string& path)
{
    close();

    AVFormatContext* rawFmt = nullptr;
    if (avformat_open_input(&rawFmt, path.c_str(), nullptr, nullptr) < 0) {
        return false;
    }
    m_format.reset(rawFmt);

    if (avformat_find_stream_info(m_format.get(), nullptr) < 0) {
        close();
        return false;
    }

    // decoder_ret differs in constness between FFmpeg versions, so look the decoder up ourselves
    m_streamIndex = av_find_best_stream(m_format.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    const AVCodec* codec = m_streamIndex >= 0
                           ? avcodec_find_decoder(m_format->streams[m_streamIndex]->codecpar->codec_id) : nullptr;
    if (m_streamIndex < 0 || !codec) {
        close();
        return false;
    }

    // Never touch other streams (audio etc.)
    for (unsigned i = 0; i < m_format->nb_streams; ++i) {
        if (static_cast<int>(i) != m_streamIndex) {
            m_format->streams[i]->discard = AVDISCARD_ALL;
        }
    }

    AVStream* stream = m_format->streams[m_streamIndex];

    m_codec.reset(avcodec_alloc_context3(codec));
    if (!m_codec || avcodec_parameters_to_context(m_codec.get(), stream->codecpar) < 0
        || avcodec_open2(m_codec.get(), codec, nullptr) < 0) {
        close();
        return false;
    }

    m_frame.reset(av_frame_alloc());
    m_prevFrame.reset(av_frame_alloc());
    m_packet.reset(av_packet_alloc());
    if (!m_frame || !m_prevFrame || !m_packet) {
        close();
        return false;
    }

    const int w = m_codec->width;
    const int h = m_codec->height;
    if (w <= 0 || h <= 0) {
        close();
        return false;
    }

    m_timeBase = av_q2d(stream->time_base);
    if (m_timeBase <= 0.0) {
        close();
        return false;
    }
    m_startPts = stream->start_time != AV_NOPTS_VALUE ? stream->start_time : 0;

    AVRational fr = av_guess_frame_rate(m_format.get(), stream, nullptr);
    if (fr.num <= 0 || fr.den <= 0) {
        fr = stream->avg_frame_rate;
    }
    if (fr.num <= 0 || fr.den <= 0) {
        fr = stream->r_frame_rate;
    }
    m_fps = (fr.num > 0 && fr.den > 0) ? av_q2d(fr) : 25.0;

    if (stream->duration != AV_NOPTS_VALUE && stream->duration > 0) {
        m_duration = stream->duration * m_timeBase;
    } else if (m_format->duration != AV_NOPTS_VALUE && m_format->duration > 0) {
        m_duration = static_cast<double>(m_format->duration) / AV_TIME_BASE;
    } else {
        m_duration = 0.0;
    }

    if (stream->nb_frames > 0) {
        m_frameCount = stream->nb_frames;
    } else {
        m_frameCount = static_cast<int64_t>(std::llround(m_duration * m_fps));
    }

    const double scale = std::min(1.0, static_cast<double>(m_maxSide) / std::max(w, h));
    m_outSize = QSize(std::max(1, static_cast<int>(std::lround(w * scale))),
                      std::max(1, static_cast<int>(std::lround(h * scale))));

    m_draining = false;
    m_cache = QImage();
    return true;
}

void FfmpegVideoDecoder::close()
{
    m_sws.reset();
    m_packet.reset();
    m_prevFrame.reset();
    m_frame.reset();
    m_codec.reset();
    m_format.reset();
    m_streamIndex = -1;
    m_fps = 0.0;
    m_duration = 0.0;
    m_frameCount = 0;
    m_startPts = 0;
    m_timeBase = 0.0;
    m_outSize = QSize();
    m_draining = false;
    m_cache = QImage();
    m_cachePts = m_cacheEnd = 0;
}

bool FfmpegVideoDecoder::isOpen() const
{
    return m_format != nullptr && m_codec != nullptr;
}

double FfmpegVideoDecoder::fps() const
{
    return m_fps;
}

int64_t FfmpegVideoDecoder::frameCount() const
{
    return m_frameCount;
}

double FfmpegVideoDecoder::duration() const
{
    return m_duration;
}

QSize FfmpegVideoDecoder::frameSize() const
{
    return m_outSize;
}

bool FfmpegVideoDecoder::seekTo(int64_t pts)
{
    m_cache = QImage();
    m_draining = false;
    if (av_seek_frame(m_format.get(), m_streamIndex, pts, AVSEEK_FLAG_BACKWARD) < 0) {
        return false;
    }
    avcodec_flush_buffers(m_codec.get());
    av_frame_unref(m_prevFrame.get());
    return true;
}

QImage FfmpegVideoDecoder::convert(const AVFrame* frame)
{
    if (frame->width <= 0 || frame->height <= 0 || frame->format == AV_PIX_FMT_NONE) {
        return QImage();
    }

    SwsContext* ctx = sws_getCachedContext(m_sws.get(), frame->width, frame->height,
                                           static_cast<AVPixelFormat>(frame->format),
                                           m_outSize.width(), m_outSize.height(),
                                           AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!ctx) {
        return QImage();
    }
    if (ctx != m_sws.get()) {
        // sws_getCachedContext freed the old one (if any) and returned a new one
        (void)m_sws.release();
        m_sws.reset(ctx);
    }

    QImage img(m_outSize, QImage::Format_RGB32);
    if (img.isNull()) {
        return QImage();
    }
    uint8_t* dst[4] = { img.bits(), nullptr, nullptr, nullptr };
    int dstStride[4] = { static_cast<int>(img.bytesPerLine()), 0, 0, 0 };
    const int h = sws_scale(m_sws.get(), frame->data, frame->linesize, 0, frame->height, dst, dstStride);
    if (h <= 0) {
        return QImage();
    }
    return img;
}

bool FfmpegVideoDecoder::decodeUntil(int64_t targetPts, QImage& out)
{
    const int64_t defaultDur = std::max<int64_t>(1, std::llround(1.0 / (m_fps * m_timeBase)));

    auto accept = [&](AVFrame* f) {
        QImage img = convert(f);
        if (img.isNull()) {
            return false;
        }
        int64_t pts = f->best_effort_timestamp;
        if (pts == AV_NOPTS_VALUE) {
            pts = f->pts;
        }
        int64_t dur = AU_FRAME_DURATION(f) > 0 ? AU_FRAME_DURATION(f) : defaultDur;
        m_cache = img;
        m_cachePts = pts;
        m_cacheEnd = pts + dur;
        out = img;
        return true;
    };

    bool havePrev = false;

    for (;;) {
        // Drain decoder output
        int ret = avcodec_receive_frame(m_codec.get(), m_frame.get());
        if (ret == 0) {
            int64_t pts = m_frame->best_effort_timestamp;
            if (pts == AV_NOPTS_VALUE) {
                pts = m_frame->pts;
            }
            const int64_t dur = AU_FRAME_DURATION(m_frame) > 0 ? AU_FRAME_DURATION(m_frame) : defaultDur;
            if (pts == AV_NOPTS_VALUE || pts + dur > targetPts) {
                const bool ok = accept(m_frame.get());
                av_frame_unref(m_frame.get());
                return ok;
            }
            av_frame_unref(m_prevFrame.get());
            av_frame_move_ref(m_prevFrame.get(), m_frame.get());
            havePrev = true;
            continue;
        }
        if (ret == AVERROR_EOF) {
            // Target is past the last frame: use the last one we saw
            if (havePrev) {
                const bool ok = accept(m_prevFrame.get());
                av_frame_unref(m_prevFrame.get());
                return ok;
            }
            return false;
        }
        if (ret != AVERROR(EAGAIN)) {
            return false;
        }

        // Need more input
        if (m_draining) {
            return false;
        }
        const int rr = av_read_frame(m_format.get(), m_packet.get());
        if (rr < 0) {
            m_draining = true;
            avcodec_send_packet(m_codec.get(), nullptr);
            continue;
        }
        if (m_packet->stream_index == m_streamIndex) {
            const int sr = avcodec_send_packet(m_codec.get(), m_packet.get());
            av_packet_unref(m_packet.get());
            if (sr < 0 && sr != AVERROR(EAGAIN) && sr != AVERROR_INVALIDDATA) {
                return false;
            }
        } else {
            av_packet_unref(m_packet.get());
        }
    }
}

QImage FfmpegVideoDecoder::frameAt(double secs)
{
    if (!isOpen() || !std::isfinite(secs)) {
        return QImage();
    }

    const double frameDur = 1.0 / m_fps;
    double maxT = m_duration > 0.0 ? m_duration - frameDur * 0.5 : 0.0;
    maxT = std::max(0.0, maxT);
    secs = std::clamp(secs, 0.0, maxT);

    const int64_t target = m_startPts + static_cast<int64_t>(std::llround(secs / m_timeBase));

    // Same frame as last time
    if (!m_cache.isNull() && target >= m_cachePts && target < m_cacheEnd) {
        return m_cache;
    }

    const int64_t frameDurPts = std::max<int64_t>(1, std::llround(frameDur / m_timeBase));
    const bool haveCursor = !m_cache.isNull();
    const bool canContinue = haveCursor && target >= m_cacheEnd
                             && (target - m_cacheEnd) <= MAX_FRAMES_AHEAD_NO_SEEK * frameDurPts;

    if (!canContinue && !seekTo(target)) {
        return QImage();
    }

    QImage out;
    if (!decodeUntil(target, out)) {
        m_cache = QImage();
        return QImage();
    }
    return out;
}
