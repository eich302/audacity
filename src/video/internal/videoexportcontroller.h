/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "framework/global/async/asyncable.h"
#include "framework/global/modularity/ioc.h"
#include "framework/global/progress.h"
#include "framework/actions/actionable.h"
#include "framework/actions/iactionsdispatcher.h"
#include "framework/interactive/iinteractive.h"
#include "framework/rcommand/commandable.h"
#include "framework/rcommand/icommanddispatcher.h"

#include "context/iglobalcontext.h"
#include "importexport/export/iexportconfiguration.h"
#include "importexport/export/iexporter.h"

#include "../ivideoservice.h"
#include "ffmpegvideoexporter.h"

namespace au::video {
class VideoExportController : public muse::actions::Actionable, public muse::rcommand::Commandable,
    public muse::async::Asyncable, public muse::Contextable
{
    muse::GlobalInject<importexport::IExportConfiguration> exportConfiguration;

    muse::ContextInject<muse::actions::IActionsDispatcher> dispatcher{ this };
    muse::ContextInject<muse::rcommand::ICommandDispatcher> commandDispatcher{ this };
    muse::ContextInject<muse::IInteractive> interactive{ this };
    muse::ContextInject<context::IGlobalContext> globalContext{ this };
    muse::ContextInject<IVideoService> videoService{ this };
    muse::ContextInject<importexport::IExporter> exporter{ this };

public:
    VideoExportController(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    void init();
    void deinit();

private:
    struct JobState {
        std::atomic<bool> canceled{ false };
    };

    muse::Ret exportVideo();
    muse::Ret renderAudioMix(const std::string& wavPath);
    void finishJob(const muse::ProgressPtr& progress, const std::string& outputPath, const std::string& wavPath, bool ok,
                   bool canceled, const std::string& error);

    bool m_busy = false;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
}
