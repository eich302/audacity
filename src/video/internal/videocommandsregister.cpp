/*
* Audacity: A Digital Audio Editor
*/
#include "videocommandsregister.h"

#include "framework/global/types/translatablestring.h"

#include "../videocommands.h"

using namespace au::video;
using namespace muse;
using namespace muse::rcommand;

namespace {
const std::vector<CommandInfo> s_commandInfos = {
    CommandInfo{
        VIDEO_EXPORT_COMMAND,
        //: Command title: shown as a menu item or a button label; keep it short
        TranslatableString("command", "Export &video…"),
        //: Command description: shown as a tooltip; can be a full sentence
        TranslatableString("command_description", "Export the project as a video file"),
        InputSchema(),
        Decoration()
    },
};
}

std::string VideoCommandsRegister::moduleName() const
{
    return "video";
}

const std::vector<Command>& VideoCommandsRegister::commandList() const
{
    static std::vector<Command> commands;
    if (commands.empty()) {
        for (const auto& info : s_commandInfos) {
            commands.push_back(info.command);
        }
    }
    return commands;
}

const std::vector<CommandInfo>& VideoCommandsRegister::commandInfoList() const
{
    return s_commandInfos;
}
