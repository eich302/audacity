/*
* Audacity: A Digital Audio Editor
*/
#include "videocommandsstate.h"

#include "../videocommands.h"

using namespace au::video;
using namespace muse::rcommand;

std::string VideoCommandsState::moduleName() const
{
    return "video";
}

void VideoCommandsState::init()
{
    globalContext()->currentProjectChanged().onNotify(this, [this]() { updateCommandStates(); });
    videoService()->videoChanged().onNotify(this, [this]() { updateCommandStates(); });
    updateCommandStates();
}

void VideoCommandsState::deinit()
{
    globalContext()->currentProjectChanged().disconnect(this);
    videoService()->videoChanged().disconnect(this);
}

void VideoCommandsState::updateCommandStates()
{
    const CommandState state = commandState(VIDEO_EXPORT_COMMAND);
    if (m_states[VIDEO_EXPORT_COMMAND] != state) {
        m_states[VIDEO_EXPORT_COMMAND] = state;
        m_stateChanged.send(VIDEO_EXPORT_COMMAND, state);
    }
}

CommandState VideoCommandsState::commandState(const Command& command) const
{
    if (command == VIDEO_EXPORT_COMMAND) {
        return CommandState(globalContext()->currentProject() != nullptr && videoService()->hasVideo(), false);
    }
    return CommandState(true, false);
}

muse::async::Channel<Command, CommandState> VideoCommandsState::commandStateChanged() const
{
    return m_stateChanged;
}
