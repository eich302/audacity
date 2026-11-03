/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <memory>
#include <mutex>
#include <set>

#include "framework/global/async/asyncable.h"
#include "framework/global/modularity/ioc.h"
#include "framework/interactive/iinteractive.h"

#include "context/iglobalcontext.h"

#include "au3-utility/Observer.h"

class AudacityProject;

#include "../ivideodecoder.h"
#include "../ivideoservice.h"

namespace au::video {
class VideoService : public IVideoService, public muse::Contextable, public muse::async::Asyncable
{
    muse::ContextInject<context::IGlobalContext> globalContext{ this };
    muse::ContextInject<muse::IInteractive> interactive{ this };

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

    void beginImport() override;
    void finishImport(const muse::io::path_t& path) override;
    std::optional<double> sourceTimeAt(double projectTime) const override;
    muse::async::Notification mappingChanged() const override;

private:
    void onCurrentProjectChanged();

    void subscribeToProjectHistory(::AudacityProject* project);

    mutable std::mutex m_mutex;
    std::unique_ptr<IVideoDecoder> m_decoder;
    muse::io::path_t m_path;
    muse::async::Notification m_videoChanged;
    muse::async::Notification m_mappingChanged;
    std::set<int64_t> m_importSnapshot;
    Observer::Subscription m_undoSubscription;
};
}
