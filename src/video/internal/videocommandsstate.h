/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <map>

#include "framework/global/async/asyncable.h"
#include "framework/global/modularity/ioc.h"
#include "framework/rcommand/imodulecommandsstate.h"

#include "context/iglobalcontext.h"

#include "../ivideoservice.h"

namespace au::video {
class VideoCommandsState : public muse::rcommand::IModuleCommandsState, public muse::Contextable, public muse::async::Asyncable
{
    muse::ContextInject<context::IGlobalContext> globalContext{ this };
    muse::ContextInject<IVideoService> videoService{ this };

public:
    VideoCommandsState(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    std::string moduleName() const override;

    void init() override;
    void deinit() override;

    muse::rcommand::CommandState commandState(const muse::rcommand::Command& command) const override;
    muse::async::Channel<muse::rcommand::Command, muse::rcommand::CommandState> commandStateChanged() const override;

private:
    void updateCommandStates();

    std::map<muse::rcommand::Command, muse::rcommand::CommandState> m_states;
    muse::async::Channel<muse::rcommand::Command, muse::rcommand::CommandState> m_stateChanged;
};
}
