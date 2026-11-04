/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <memory>

#include "framework/global/async/asyncable.h"
#include "framework/global/modularity/ioc.h"
#include "framework/ui/iuiactionsmodule.h"

#include "../ivideoservice.h"

namespace au::video {
//! Registers the "video-export" ui action so that it can have a shortcut and a menu title
class VideoUiActions final : public muse::ui::IUiActionsModule, public muse::async::Asyncable, public muse::Contextable
{
    muse::ContextInject<IVideoService> videoService { this };

public:
    VideoUiActions(const muse::modularity::ContextPtr& ctx);

    void init();

    const muse::ui::UiActionList& actionsList() const override;

    bool actionEnabled(const muse::ui::UiAction& act) const override;
    muse::async::Channel<muse::actions::ActionCodeList> actionEnabledChanged() const override;

    bool actionChecked(const muse::ui::UiAction& act) const override;
    muse::async::Channel<muse::actions::ActionCodeList> actionCheckedChanged() const override;

private:
    static const muse::ui::UiActionList m_actions;

    muse::async::Channel<muse::actions::ActionCodeList> m_actionEnabledChanged;
    muse::async::Channel<muse::actions::ActionCodeList> m_actionCheckedChanged;
};
}
