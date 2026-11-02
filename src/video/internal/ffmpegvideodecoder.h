/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <cstdint>
#include <memory>

#include "../ivideodecoder.h"

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace au::video {
class FfmpegVideoDecoder : public IVideoDecoder
{
public:
    FfmpegVideoDecoder();
    ~FfmpegVideoDecoder() override;

    bool open(const std::string& path) override;
    void close() override;
    bool isOpen() const override;

    double fps() const override;
    int64_t frameCount() const override;
    double duration() const override;

    QImage frameAt(double secs) override;
    QSize frameSize() const override;

private:
    struct FormatDeleter { void operator()(AVFormatContext* p) const; };
    struct CodecDeleter { void operator()(AVCodecContext* p) const; };
    struct FrameDeleter { void operator()(AVFrame* p) const; };
    struct PacketDeleter { void operator()(AVPacket* p) const; };
    struct SwsDeleter { void operator()(SwsContext* p) const; };

    bool seekTo(int64_t pts);
    bool decodeUntil(int64_t targetPts, QImage& out);
    QImage convert(const AVFrame* frame);

    std::unique_ptr<AVFormatContext, FormatDeleter> m_format;
    std::unique_ptr<AVCodecContext, CodecDeleter> m_codec;
    std::unique_ptr<AVFrame, FrameDeleter> m_frame;
    std::unique_ptr<AVFrame, FrameDeleter> m_prevFrame;
    std::unique_ptr<AVPacket, PacketDeleter> m_packet;
    std::unique_ptr<SwsContext, SwsDeleter> m_sws;

    int m_streamIndex = -1;
    double m_fps = 0.0;
    double m_duration = 0.0;
    int64_t m_frameCount = 0;
    int64_t m_startPts = 0;
    double m_timeBase = 0.0;
    QSize m_outSize;

    bool m_draining = false;

    // Cache of the last returned frame: valid for pts in [m_cachePts, m_cacheEnd)
    QImage m_cache;
    int64_t m_cachePts = 0;
    int64_t m_cacheEnd = 0;
};
}
