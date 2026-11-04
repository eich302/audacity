/*
* Audacity: A Digital Audio Editor
*/
#include "videouiactions.h"

#include "context/uicontext.h"
#include "context/shortcutcontext.h"
#include "types/translatablestring.h"

#include "../videocommands.h"

using namespace muse;
using namespace muse::ui;
using namespace au::video;

const UiActionList VideoUiActions::m_actions = {
    UiAction(VIDEO_EXPORT_ACTION,
             au::context::UiCtxProjectOpened,
             au::context::CTX_ANY,
             //: Action title: shown as a menu item or a button label; keep it short
             TranslatableString("action", "Export &video…"),
             //: Action description: shown as a tooltip; can be a full sentence
             TranslatableString("action_description", "Export the edited project as a video file")
             ),
};

VideoUiActions::VideoUiActions(const muse::modularity::ContextPtr& ctx)
    : muse::Contextable(ctx)
{
}

void VideoUiActions::init()
{
    if (!videoService()) {
        return;
    }

    videoService()->videoChanged().onNotify(this, [this]() {
        m_actionEnabledChanged.send({ VIDEO_EXPORT_ACTION });
    });
}

const UiActionList& VideoUiActions::actionsList() const
{
    return m_actions;
}

bool VideoUiActions::actionEnabled(const UiAction&) const
{
    return videoService() && videoService()->hasVideo();
}

muse::async::Channel<muse::actions::ActionCodeList> VideoUiActions::actionEnabledChanged() const
{
    return m_actionEnabledChanged;
}

bool VideoUiActions::actionChecked(const UiAction&) const
{
    return false;
}

muse::async::Channel<muse::actions::ActionCodeList> VideoUiActions::actionCheckedChanged() const
{
    return m_actionCheckedChanged;
}
