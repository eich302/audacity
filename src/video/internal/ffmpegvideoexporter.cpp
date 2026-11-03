/*
* Audacity: A Digital Audio Editor
*/
#include "ffmpegvideoexporter.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>

#include <QImage>
#include <QtGlobal>

#include "ffmpegvideodecoder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

using namespace au::video;

namespace {
struct FormatOutDeleter {
    void operator()(AVFormatContext* p) const
    {
        if (!p) {
            return;
        }
        if (p->pb && !(p->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&p->pb);
        }
        avformat_free_context(p);
    }
};
struct FormatInDeleter { void operator()(AVFormatContext* p) const { avformat_close_input(&p); } };
struct CodecDeleter { void operator()(AVCodecContext* p) const { avcodec_free_context(&p); } };
struct FrameDeleter { void operator()(AVFrame* p) const { av_frame_free(&p); } };
struct PacketDeleter { void operator()(AVPacket* p) const { av_packet_free(&p); } };
struct SwrDeleter { void operator()(SwrContext* p) const { swr_free(&p); } };
struct SwsDeleter { void operator()(SwsContext* p) const { sws_freeContext(p); } };
struct FifoDeleter { void operator()(AVAudioFifo* p) const { av_audio_fifo_free(p); } };

using FormatOutPtr = std::unique_ptr<AVFormatContext, FormatOutDeleter>;
using FormatInPtr = std::unique_ptr<AVFormatContext, FormatInDeleter>;
using CodecPtr = std::unique_ptr<AVCodecContext, CodecDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using SwrPtr = std::unique_ptr<SwrContext, SwrDeleter>;
using SwsPtr = std::unique_ptr<SwsContext, SwsDeleter>;
using FifoPtr = std::unique_ptr<AVAudioFifo, FifoDeleter>;

std::string av_err(int err)
{
    char buf[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(err, buf, sizeof(buf));
    return buf;
}

std::string lowerExt(const std::string& path)
{
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!ext.empty() && ext[0] == '.') {
        ext.erase(0, 1);
    }
    return ext;
}

// ---- supported configs (FFmpeg 7.1+ API with fallback to the deprecated fields) ----

std::vector<AVPixelFormat> supportedPixFmts(const AVCodecContext* ctx, const AVCodec* codec)
{
    std::vector<AVPixelFormat> result;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100)
    const void* configs = nullptr;
    int num = 0;
    if (avcodec_get_supported_config(ctx, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &configs, &num) >= 0 && configs) {
        const AVPixelFormat* fmts = static_cast<const AVPixelFormat*>(configs);
        for (int i = 0; i < num && fmts[i] != AV_PIX_FMT_NONE; ++i) {
            result.push_back(fmts[i]);
        }
    }
#else
    (void)ctx;
    if (codec->pix_fmts) {
        for (const AVPixelFormat* p = codec->pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
            result.push_back(*p);
        }
    }
#endif
    return result;
}

std::vector<AVSampleFormat> supportedSampleFmts(const AVCodecContext* ctx, const AVCodec* codec)
{
    std::vector<AVSampleFormat> result;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100)
    const void* configs = nullptr;
    int num = 0;
    if (avcodec_get_supported_config(ctx, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &configs, &num) >= 0 && configs) {
        const AVSampleFormat* fmts = static_cast<const AVSampleFormat*>(configs);
        for (int i = 0; i < num && fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
            result.push_back(fmts[i]);
        }
    }
#else
    (void)ctx;
    if (codec->sample_fmts) {
        for (const AVSampleFormat* p = codec->sample_fmts; *p != AV_SAMPLE_FMT_NONE; ++p) {
            result.push_back(*p);
        }
    }
#endif
    return result;
}

std::vector<int> supportedSampleRates(const AVCodecContext* ctx, const AVCodec* codec)
{
    std::vector<int> result;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100)
    const void* configs = nullptr;
    int num = 0;
    if (avcodec_get_supported_config(ctx, codec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, &configs, &num) >= 0 && configs) {
        const int* rates = static_cast<const int*>(configs);
        for (int i = 0; i < num && rates[i] != 0; ++i) {
            result.push_back(rates[i]);
        }
    }
#else
    (void)ctx;
    if (codec->supported_samplerates) {
        for (const int* p = codec->supported_samplerates; *p != 0; ++p) {
            result.push_back(*p);
        }
    }
#endif
    return result;
}

// ---- encoder selection ----

std::vector<std::string> videoEncoderCandidates(const std::string& container, VideoCodec codec)
{
    if (container == "webm") {
        return { "libvpx-vp9", "libvpx", "libaom-av1" };
    }
    if (codec == VideoCodec::H265 && (container == "mp4" || container == "mov" || container == "mkv")) {
        return { "libx265", "hevc_mf", "hevc_nvenc", "hevc_qsv", "hevc_amf" };
    }
    if (container == "avi") {
        return { "libx264", "libopenh264", "h264_mf", "mpeg4" };
    }
    return { "libx264", "libopenh264", "h264_mf", "mpeg4" };
}

std::vector<std::string> audioEncoderCandidates(const std::string& container)
{
    if (container == "webm") {
        return { "libopus", "opus", "libvorbis" };
    }
    if (container == "avi") {
        return { "libmp3lame", "ac3", "pcm_s16le" };
    }
    return { "aac" };
}

bool isHardwareHevc(const std::string& name)
{
    return name == "hevc_mf" || name == "hevc_nvenc" || name == "hevc_qsv" || name == "hevc_amf";
}

AVPixelFormat choosePixFmt(const std::vector<AVPixelFormat>& supported)
{
    if (supported.empty()) {
        return AV_PIX_FMT_YUV420P;
    }
    if (std::find(supported.begin(), supported.end(), AV_PIX_FMT_YUV420P) != supported.end()) {
        return AV_PIX_FMT_YUV420P;
    }
    return supported.front();
}

CodecPtr openVideoEncoder(const AVCodec* codec, const std::string& name, int width, int height, AVRational timeBase,
                          AVRational frameRate, double fps, bool globalHeader, std::string& err)
{
    CodecPtr ctx(avcodec_alloc_context3(codec));
    if (!ctx) {
        err = "Could not allocate video encoder context";
        return nullptr;
    }

    ctx->width = width;
    ctx->height = height;
    ctx->time_base = timeBase;
    ctx->framerate = frameRate;
    ctx->sample_aspect_ratio = AVRational { 1, 1 };
    ctx->pix_fmt = choosePixFmt(supportedPixFmts(ctx.get(), codec));
    ctx->gop_size = std::max(1, static_cast<int>(std::lround(fps * 2.0)));
    if (globalHeader) {
        ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVDictionary* opts = nullptr;
    bool useCrf = true;

    if (name == "libx264") {
        av_opt_set(ctx->priv_data, "crf", "20", 0);
        av_opt_set(ctx->priv_data, "preset", "medium", 0);
    } else if (name == "libx265") {
        av_opt_set(ctx->priv_data, "crf", "24", 0);
        av_opt_set(ctx->priv_data, "preset", "medium", 0);
        av_opt_set(ctx->priv_data, "x265-params", "log-level=error", 0);
    } else if (name == "libvpx-vp9") {
        ctx->bit_rate = 0;
        av_opt_set(ctx->priv_data, "crf", "32", 0);
        av_opt_set(ctx->priv_data, "deadline", "good", 0);
        av_opt_set(ctx->priv_data, "cpu-used", "4", 0);
        av_opt_set(ctx->priv_data, "row-mt", "1", 0);
    } else if (name == "libaom-av1") {
        ctx->bit_rate = 0;
        av_opt_set(ctx->priv_data, "crf", "30", 0);
        av_opt_set(ctx->priv_data, "cpu-used", "6", 0);
        av_opt_set(ctx->priv_data, "row-mt", "1", 0);
    } else {
        useCrf = false;
        ctx->bit_rate = isHardwareHevc(name) ? 5000000 : 8000000;
    }
    (void)useCrf;

    const int ret = avcodec_open2(ctx.get(), codec, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        err = "Could not open video encoder " + name + ": " + av_err(ret);
        return nullptr;
    }
    return ctx;
}

CodecPtr openAudioEncoder(const AVCodec* codec, const std::string& name, int wavRate, bool globalHeader, std::string& err)
{
    CodecPtr ctx(avcodec_alloc_context3(codec));
    if (!ctx) {
        err = "Could not allocate audio encoder context";
        return nullptr;
    }

    const std::vector<AVSampleFormat> fmts = supportedSampleFmts(ctx.get(), codec);
    ctx->sample_fmt = fmts.empty() ? AV_SAMPLE_FMT_FLTP : fmts.front();

    const std::vector<int> rates = supportedSampleRates(ctx.get(), codec);
    int rate = wavRate > 0 ? wavRate : 44100;
    if (!rates.empty() && std::find(rates.begin(), rates.end(), rate) == rates.end()) {
        if (std::find(rates.begin(), rates.end(), 48000) != rates.end()) {
            rate = 48000;
        } else if (std::find(rates.begin(), rates.end(), 44100) != rates.end()) {
            rate = 44100;
        } else {
            rate = rates.front();
        }
    }
    ctx->sample_rate = rate;
    ctx->time_base = AVRational { 1, rate };
    av_channel_layout_default(&ctx->ch_layout, 2);
    if (name != "pcm_s16le") {
        ctx->bit_rate = (name == "aac") ? 192000 : 160000;
    }
    if (globalHeader) {
        ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    const int ret = avcodec_open2(ctx.get(), codec, nullptr);
    if (ret < 0) {
        err = "Could not open audio encoder " + name + ": " + av_err(ret);
        return nullptr;
    }
    return ctx;
}

bool encodeAndWrite(AVCodecContext* enc, AVStream* stream, AVFormatContext* oc, AVFrame* frame, AVPacket* pkt, std::string& err)
{
    int ret = avcodec_send_frame(enc, frame);
    if (ret < 0 && ret != AVERROR_EOF) {
        err = "Error sending frame to encoder: " + av_err(ret);
        return false;
    }
    while (true) {
        ret = avcodec_receive_packet(enc, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return true;
        }
        if (ret < 0) {
            err = "Error encoding: " + av_err(ret);
            return false;
        }
        av_packet_rescale_ts(pkt, enc->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        ret = av_interleaved_write_frame(oc, pkt);
        if (ret < 0) {
            err = "Error writing packet: " + av_err(ret);
            return false;
        }
    }
}

// Decodes a WAV, resamples to the audio encoder format and encodes it in time-ordered chunks
class AudioPump
{
public:
    bool init(const std::string& wavPath, AVCodecContext* enc, AVStream* stream, AVFormatContext* oc, std::string& err)
    {
        m_enc = enc;
        m_stream = stream;
        m_oc = oc;

        AVFormatContext* fmt = nullptr;
        int ret = avformat_open_input(&fmt, wavPath.c_str(), nullptr, nullptr);
        if (ret < 0) {
            err = "Could not open audio file: " + av_err(ret);
            return false;
        }
        m_in.reset(fmt);
        ret = avformat_find_stream_info(m_in.get(), nullptr);
        if (ret < 0) {
            err = "Could not read audio file info: " + av_err(ret);
            return false;
        }
        m_streamIndex = av_find_best_stream(m_in.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (m_streamIndex < 0) {
            err = "No audio stream in the rendered audio file";
            return false;
        }
        AVStream* st = m_in->streams[m_streamIndex];
        const AVCodec* dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            err = "No decoder for the rendered audio file";
            return false;
        }
        m_dec.reset(avcodec_alloc_context3(dec));
        avcodec_parameters_to_context(m_dec.get(), st->codecpar);
        ret = avcodec_open2(m_dec.get(), dec, nullptr);
        if (ret < 0) {
            err = "Could not open audio decoder: " + av_err(ret);
            return false;
        }

        AVChannelLayout inLayout = {};
        if (m_dec->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC || m_dec->ch_layout.nb_channels == 0) {
            av_channel_layout_default(&inLayout, std::max(1, m_dec->ch_layout.nb_channels));
        } else {
            av_channel_layout_copy(&inLayout, &m_dec->ch_layout);
        }

        SwrContext* swr = nullptr;
        ret = swr_alloc_set_opts2(&swr, &m_enc->ch_layout, m_enc->sample_fmt, m_enc->sample_rate,
                                  &inLayout, m_dec->sample_fmt, m_dec->sample_rate, 0, nullptr);
        av_channel_layout_uninit(&inLayout);
        if (ret < 0 || !swr) {
            err = "Could not create audio resampler";
            return false;
        }
        m_swr.reset(swr);
        ret = swr_init(m_swr.get());
        if (ret < 0) {
            err = "Could not initialize audio resampler: " + av_err(ret);
            return false;
        }

        m_fifo.reset(av_audio_fifo_alloc(m_enc->sample_fmt, m_enc->ch_layout.nb_channels, 1));
        if (!m_fifo) {
            err = "Could not allocate audio FIFO";
            return false;
        }

        m_packet.reset(av_packet_alloc());
        m_inPacket.reset(av_packet_alloc());
        m_inFrame.reset(av_frame_alloc());
        m_frameSize = m_enc->frame_size > 0 ? m_enc->frame_size : 1024;
        return true;
    }

    // Encode until at least `untilSecs` of audio have been written (or the input is exhausted)
    bool pump(double untilSecs, std::string& err)
    {
        const int64_t targetSamples = untilSecs >= 1e9
                                      ? std::numeric_limits<int64_t>::max()
                                      : static_cast<int64_t>(untilSecs * m_enc->sample_rate);
        while (m_samplesWritten < targetSamples) {
            if (av_audio_fifo_size(m_fifo.get()) >= m_frameSize) {
                if (!encodeFromFifo(m_frameSize, err)) {
                    return false;
                }
                continue;
            }
            if (m_eof) {
                const int remaining = av_audio_fifo_size(m_fifo.get());
                if (remaining > 0) {
                    if (!encodeFromFifo(remaining, err)) {
                        return false;
                    }
                    continue;
                }
                break;
            }
            if (!readMore(err)) {
                return false;
            }
        }
        return true;
    }

    bool flush(std::string& err)
    {
        if (!pump(1e12, err)) {
            return false;
        }
        return encodeAndWrite(m_enc, m_stream, m_oc, nullptr, m_packet.get(), err);
    }

private:
    bool readMore(std::string& err)
    {
        int ret = av_read_frame(m_in.get(), m_inPacket.get());
        if (ret == AVERROR_EOF || (ret < 0 && ret != AVERROR(EAGAIN))) {
            // flush the decoder, then the resampler
            avcodec_send_packet(m_dec.get(), nullptr);
            if (!drainDecoder(err)) {
                return false;
            }
            if (!convertAndStore(nullptr, 0, err)) {
                return false;
            }
            m_eof = true;
            return true;
        }
        if (m_inPacket->stream_index == m_streamIndex) {
            ret = avcodec_send_packet(m_dec.get(), m_inPacket.get());
            av_packet_unref(m_inPacket.get());
            if (ret < 0 && ret != AVERROR(EAGAIN)) {
                err = "Audio decode error: " + av_err(ret);
                return false;
            }
            return drainDecoder(err);
        }
        av_packet_unref(m_inPacket.get());
        return true;
    }

    bool drainDecoder(std::string& err)
    {
        while (true) {
            const int ret = avcodec_receive_frame(m_dec.get(), m_inFrame.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return true;
            }
            if (ret < 0) {
                err = "Audio decode error: " + av_err(ret);
                return false;
            }
            const bool ok = convertAndStore(m_inFrame->extended_data, m_inFrame->nb_samples, err);
            av_frame_unref(m_inFrame.get());
            if (!ok) {
                return false;
            }
        }
    }

    bool convertAndStore(uint8_t** inData, int inSamples, std::string& err)
    {
        const int channels = m_enc->ch_layout.nb_channels;
        const int maxOut = static_cast<int>(av_rescale_rnd(swr_get_delay(m_swr.get(), m_dec->sample_rate) + inSamples,
                                                           m_enc->sample_rate, m_dec->sample_rate, AV_ROUND_UP)) + 32;
        uint8_t** out = nullptr;
        int linesize = 0;
        int ret = av_samples_alloc_array_and_samples(&out, &linesize, channels, maxOut, m_enc->sample_fmt, 0);
        if (ret < 0) {
            err = "Out of memory (audio)";
            return false;
        }
        const int converted = swr_convert(m_swr.get(), out, maxOut, const_cast<const uint8_t**>(inData), inSamples);
        bool ok = true;
        if (converted < 0) {
            err = "Audio resampling error: " + av_err(converted);
            ok = false;
        } else if (converted > 0) {
            ret = av_audio_fifo_write(m_fifo.get(), reinterpret_cast<void**>(out), converted);
            if (ret < converted) {
                err = "Could not write to audio FIFO";
                ok = false;
            }
        }
        if (out) {
            av_freep(&out[0]);
        }
        av_freep(&out);
        return ok;
    }

    bool encodeFromFifo(int samples, std::string& err)
    {
        FramePtr frame(av_frame_alloc());
        frame->nb_samples = samples;
        frame->format = m_enc->sample_fmt;
        frame->sample_rate = m_enc->sample_rate;
        av_channel_layout_copy(&frame->ch_layout, &m_enc->ch_layout);
        int ret = av_frame_get_buffer(frame.get(), 0);
        if (ret < 0) {
            err = "Could not allocate audio frame: " + av_err(ret);
            return false;
        }
        const int read = av_audio_fifo_read(m_fifo.get(), reinterpret_cast<void**>(frame->data), samples);
        if (read < samples) {
            err = "Could not read from audio FIFO";
            return false;
        }
        frame->pts = m_samplesWritten;
        m_samplesWritten += samples;
        return encodeAndWrite(m_enc, m_stream, m_oc, frame.get(), m_packet.get(), err);
    }

    AVCodecContext* m_enc = nullptr;
    AVStream* m_stream = nullptr;
    AVFormatContext* m_oc = nullptr;
    FormatInPtr m_in;
    CodecPtr m_dec;
    SwrPtr m_swr;
    FifoPtr m_fifo;
    PacketPtr m_packet;
    PacketPtr m_inPacket;
    FramePtr m_inFrame;
    int m_streamIndex = -1;
    int m_frameSize = 1024;
    int64_t m_samplesWritten = 0;
    bool m_eof = false;
};
}

bool FfmpegVideoExporter::exportVideo(const ExportParams& params, std::function<bool(double)> progress)
{
    m_error.clear();
    m_videoEncoder.clear();
    m_canceled = false;

    namespace fs = std::filesystem;

    FormatOutPtr oc;

    auto fail = [this, &params, &oc](const std::string& msg) {
        m_error = msg;
        oc.reset(); // close the output file before deleting it
        std::error_code ec;
        fs::remove(params.outputPath, ec);
        return false;
    };

    if (params.frameSourceTimes.empty()) {
        return fail("Nothing to export");
    }
    if (params.fps <= 0.0) {
        return fail("Invalid frame rate");
    }

    const std::string container = lowerExt(params.outputPath);

    // --- source decoder ---
    FfmpegVideoDecoder decoder;
    decoder.setMaxSide(4096);
    const bool hasSource = !params.sourceVideoPath.empty() && decoder.open(params.sourceVideoPath);

    int width = params.width;
    int height = params.height;
    if (width <= 0 || height <= 0) {
        if (hasSource) {
            width = decoder.frameSize().width();
            height = decoder.frameSize().height();
        } else {
            width = 640;
            height = 360;
        }
    }
    width &= ~1;
    height &= ~1;
    if (width < 2 || height < 2) {
        return fail("Invalid video size");
    }

    // --- output ---
    AVFormatContext* rawOc = nullptr;
    int ret = avformat_alloc_output_context2(&rawOc, nullptr, nullptr, params.outputPath.c_str());
    if (ret < 0 || !rawOc) {
        return fail("Unsupported output format: " + av_err(ret));
    }
    oc.reset(rawOc);
    const bool globalHeader = (oc->oformat->flags & AVFMT_GLOBALHEADER) != 0;

    // --- frame rate / time base ---
    AVRational frameRate = av_d2q(params.fps, 100000);
    if (frameRate.num <= 0 || frameRate.den <= 0) {
        frameRate = AVRational { 25, 1 };
    }
    const AVRational timeBase = av_inv_q(frameRate);

    // --- video encoder ---
    CodecPtr venc;
    std::string lastOpenErr;
    bool anyVideoEncoderFound = false;
    for (const std::string& name : videoEncoderCandidates(container, params.videoCodec)) {
        const AVCodec* codec = avcodec_find_encoder_by_name(name.c_str());
        if (!codec) {
            continue;
        }
        anyVideoEncoderFound = true;
        std::string err;
        venc = openVideoEncoder(codec, name, width, height, timeBase, frameRate, params.fps, globalHeader, err);
        if (venc) {
            m_videoEncoder = name;
            break;
        }
        lastOpenErr = err;
    }
    if (!venc) {
        if (params.videoCodec == VideoCodec::H265 && container != "webm") {
            return fail("No H.265 encoder available on this system, please use H.264");
        }
        return fail(anyVideoEncoderFound ? lastOpenErr : "No suitable video encoder found for ." + container);
    }

    AVStream* vstream = avformat_new_stream(oc.get(), nullptr);
    if (!vstream) {
        return fail("Could not create video stream");
    }
    ret = avcodec_parameters_from_context(vstream->codecpar, venc.get());
    if (ret < 0) {
        return fail("Could not set video stream parameters: " + av_err(ret));
    }
    vstream->time_base = venc->time_base;
    vstream->avg_frame_rate = frameRate;
    if (params.videoCodec == VideoCodec::H265 && (container == "mp4" || container == "mov")
        && venc->codec_id == AV_CODEC_ID_HEVC) {
        vstream->codecpar->codec_tag = MKTAG('h', 'v', 'c', '1');
    }

    // --- audio encoder ---
    const bool withAudio = !params.audioWavPath.empty();
    CodecPtr aenc;
    AVStream* astream = nullptr;
    AudioPump audio;
    if (withAudio) {
        int wavRate = 0;
        {
            AVFormatContext* probe = nullptr;
            if (avformat_open_input(&probe, params.audioWavPath.c_str(), nullptr, nullptr) >= 0) {
                FormatInPtr holder(probe);
                if (avformat_find_stream_info(holder.get(), nullptr) >= 0) {
                    const int idx = av_find_best_stream(holder.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
                    if (idx >= 0) {
                        wavRate = holder->streams[idx]->codecpar->sample_rate;
                    }
                }
            }
        }

        std::string err;
        for (const std::string& name : audioEncoderCandidates(container)) {
            const AVCodec* codec = avcodec_find_encoder_by_name(name.c_str());
            if (!codec) {
                continue;
            }
            aenc = openAudioEncoder(codec, name, wavRate, globalHeader, err);
            if (aenc) {
                break;
            }
        }
        if (!aenc) {
            return fail(err.empty() ? "No suitable audio encoder found for ." + container : err);
        }
        astream = avformat_new_stream(oc.get(), nullptr);
        if (!astream) {
            return fail("Could not create audio stream");
        }
        ret = avcodec_parameters_from_context(astream->codecpar, aenc.get());
        if (ret < 0) {
            return fail("Could not set audio stream parameters: " + av_err(ret));
        }
        astream->time_base = aenc->time_base;
    }

    if (!(oc->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, params.outputPath.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            return fail("Could not open output file: " + av_err(ret));
        }
    }
    ret = avformat_write_header(oc.get(), nullptr);
    if (ret < 0) {
        return fail("Could not write file header: " + av_err(ret));
    }

    std::string err;
    if (withAudio && !audio.init(params.audioWavPath, aenc.get(), astream, oc.get(), err)) {
        return fail(err);
    }

    // --- video frames ---
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    const AVPixelFormat rgbFmt = AV_PIX_FMT_BGRA;
#else
    const AVPixelFormat rgbFmt = AV_PIX_FMT_ARGB;
#endif

    FramePtr vframe(av_frame_alloc());
    vframe->format = venc->pix_fmt;
    vframe->width = width;
    vframe->height = height;
    ret = av_frame_get_buffer(vframe.get(), 0);
    if (ret < 0) {
        return fail("Could not allocate video frame: " + av_err(ret));
    }
    PacketPtr vpacket(av_packet_alloc());
    SwsPtr sws;

    QImage black(width, height, QImage::Format_RGB32);
    black.fill(Qt::black);

    QImage current;
    std::optional<double> currentSrcTime;
    bool currentIsBlack = false;
    bool haveFrame = false;
    const double srcFrameDur = (hasSource && decoder.fps() > 0.0) ? 1.0 / decoder.fps() : 0.0;

    const size_t total = params.frameSourceTimes.size();
    for (size_t i = 0; i < total; ++i) {
        const std::optional<double>& st = params.frameSourceTimes[i];

        bool changed = false;
        if (!st.has_value() || !hasSource) {
            if (!haveFrame || !currentIsBlack) {
                current = black;
                currentIsBlack = true;
                currentSrcTime.reset();
                changed = true;
            }
        } else {
            const bool sameFrame = haveFrame && !currentIsBlack && currentSrcTime.has_value()
                                   && std::abs(*st - *currentSrcTime) < srcFrameDur * 0.5;
            if (!sameFrame) {
                QImage img = decoder.frameAt(*st);
                if (!img.isNull()) {
                    current = img.format() == QImage::Format_RGB32 ? img : img.convertToFormat(QImage::Format_RGB32);
                    currentSrcTime = *st;
                    currentIsBlack = false;
                    changed = true;
                } else if (!haveFrame) {
                    current = black;
                    currentIsBlack = true;
                    changed = true;
                }
            }
        }

        ret = av_frame_make_writable(vframe.get());
        if (ret < 0) {
            return fail("Could not make video frame writable: " + av_err(ret));
        }
        if (changed || !haveFrame) {
            sws.reset(sws_getCachedContext(sws.release(), current.width(), current.height(), rgbFmt, width, height,
                                           venc->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr));
            if (!sws) {
                return fail("Could not create the video scaler");
            }
            const uint8_t* srcData[4] = { current.constBits(), nullptr, nullptr, nullptr };
            const int srcStride[4] = { static_cast<int>(current.bytesPerLine()), 0, 0, 0 };
            sws_scale(sws.get(), srcData, srcStride, 0, current.height(), vframe->data, vframe->linesize);
            haveFrame = true;
        }
        vframe->pts = static_cast<int64_t>(i);

        if (!encodeAndWrite(venc.get(), vstream, oc.get(), vframe.get(), vpacket.get(), err)) {
            return fail(err);
        }

        if (withAudio && !audio.pump(static_cast<double>(i) / params.fps + 0.5, err)) {
            return fail(err);
        }

        if (progress && !progress(static_cast<double>(i + 1) / static_cast<double>(total))) {
            m_canceled = true;
            m_error.clear();
            oc.reset();
            std::error_code ec;
            fs::remove(params.outputPath, ec);
            return false;
        }
    }

    // --- flush ---
    if (!encodeAndWrite(venc.get(), vstream, oc.get(), nullptr, vpacket.get(), err)) {
        return fail(err);
    }
    if (withAudio && !audio.flush(err)) {
        return fail(err);
    }

    ret = av_write_trailer(oc.get());
    if (ret < 0) {
        return fail("Could not finalize the file: " + av_err(ret));
    }
    return true;
}
