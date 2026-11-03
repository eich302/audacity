/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <memory>
#include <mutex>

#include "framework/global/async/asyncable.h"
#include "framework/global/modularity/ioc.h"

#include "context/iglobalcontext.h"

#include "../ivideodecoder.h"
#include "../ivideoservice.h"

namespace au::video {
class VideoService : public IVideoService, public muse::Contextable, public muse::async::Asyncable
{
    muse::ContextInject<context::IGlobalContext> globalContext{ this };

public:
    VideoService(const muse::modularity::ContextPtr& ctx);
    // For tests: inject a decoder
    VideoService(const muse::modularity::ContextPtr& ctx, std::unique_ptr<IVideoDecoder> decoder);

    void init();
    void deinit();

    bool loadVideo(const muse::io::path_t& path) override;
    void unloadVideo() override;
    bool hasVideo() const override;
    muse::io::path_t videoPath() const override;
    muse::async::Notification videoChanged() const override;

    double fps() const override;
    double duration() const override;
    int64_t frameCount() const override;
    QImage frameAt(double secs) override;

    void setProjectVideoPath(const muse::io::path_t& path) override;
    bool probeHasVideo(const muse::io::path_t& path) const override;

private:
    void onCurrentProjectChanged();

    mutable std::mutex m_mutex;
    std::unique_ptr<IVideoDecoder> m_decoder;
    muse::io::path_t m_path;
    muse::async::Notification m_videoChanged;
};
}
