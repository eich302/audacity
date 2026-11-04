/*
* Audacity: A Digital Audio Editor
*/
#include "projectpagemodel.h"

#include "framework/global/log.h"
#include "framework/global/async/async.h"
#include "framework/dockwindow/dockcommands.h"

#include "playback/playbackcommands.h"
#include "projectscene/view/toolbars/playbacktoolbarconfig.h"

#include "videopanellayout.h"

using namespace au::appshell;
using namespace muse;

ProjectPageModel::ProjectPageModel(QObject* parent)
    : QObject(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

void ProjectPageModel::init()
{
    TRACEFUNC;

    //! NOTE: a dock gets added with its last size, which after a reset to the default layout is the moved panel's
    m_videoPanelSizeResetPending = uiState()->pageState("ProjectPage").val.isEmpty();

    if (!m_inited) {
        applicationActionController()->dockToggleRequested().onReceive(this, [this](const DockName& dockName) {
            toggleDock(dockName);
        });

        uiState()->toolConfigChanged(playbackToolBarName()).onNotify(this, [this]() {
            updatePlaybackMeterVisibility();
        });

        playbackUiState()->playbackMeterPositionChanged().onNotify(this, [this]() {
            updatePlaybackMeterVisibility();
        });

        //! NOTE: the video module is optional, so the service may be absent
        if (videoService()) {
            videoService()->videoChanged().onNotify(this, [this]() {
                updateVideoPanelVisibility();
            });
        }

        m_inited = true;
    }

    updatePlaybackMeterVisibility();
    updateVideoPanelVisibility();
}

void ProjectPageModel::updateVideoPanelVisibility()
{
    //! NOTE: the panel content is only instantiated when the dock is open,
    //! so opening/closing it has to be driven from here and not from the panel itself
    const bool open = videoService() && videoService()->hasVideo();
    muse::async::Async::call(this, [this, open]() {
        commandDispatcher()->dispatch(dock::DOCK_SET_OPEN_COMMAND, {
            { "dock_name", Val(VIDEO_PANEL_NAME.toStdString()) },
            { "open", Val(open) },
        });
        if (open) {
            //! NOTE: the dock is only there once the command has been processed
            muse::async::Async::call(this, [this]() {
                updateVideoPanelLocation();
            });
        }
    });
}

void ProjectPageModel::updateVideoPanelLocation()
{
    //! NOTE: derived from the layout, which is what gets saved and restored (and reset to default)
    const dock::Location location = videopanellayout::currentLocation(iocContext()->id);
    if (location != dock::Location::Undefined) {
        setVideoPanelLocation(location);

        if (m_videoPanelSizeResetPending) {
            m_videoPanelSizeResetPending = false;
            emit videoPanelSizeResetRequested();
        }
    }
}

int ProjectPageModel::videoPanelLocation() const
{
    return m_videoPanelLocation;
}

void ProjectPageModel::setVideoPanelLocation(int location)
{
    if (m_videoPanelLocation == location) {
        return;
    }

    m_videoPanelLocation = location;
    emit videoPanelLocationChanged();
}

void ProjectPageModel::moveVideoPanel(int location, int extent)
{
    if (location == m_videoPanelLocation) {
        return;
    }

    //! NOTE: first, the panel's size constraints follow this and must fit the new place before it gets there
    setVideoPanelLocation(location);
    videopanellayout::move(iocContext()->id, static_cast<dock::Location>(location), extent);
}

void ProjectPageModel::resizeVideoPanel(int extent)
{
    videopanellayout::resize(iocContext()->id, static_cast<dock::Location>(m_videoPanelLocation), extent);
}

void ProjectPageModel::updatePlaybackMeterVisibility()
{
    const auto toolConfig = uiState()->toolConfig(playbackToolBarName(), projectscene::defaultPlaybackToolBarConfig());
    const auto it = std::find_if(toolConfig.items.begin(), toolConfig.items.end(),
                                 [](const muse::ui::ToolConfig::Item& item) {
        return item.intent == playback::PLAYBACK_LEVEL_COMMAND.toString();
    });

    if (it != toolConfig.items.end()) {
        const bool meterPanelVisible = it->show
                                       && (playbackUiState()->playbackMeterPosition()
                                           == playback::PlaybackMeterPosition::MeterPosition::SideBar);
        muse::async::Async::call(this, [this, meterPanelVisible]() {
            commandDispatcher()->dispatch(dock::DOCK_SET_OPEN_COMMAND, {
                { "dock_name", Val(playbackMeterPanelName().toStdString()) },
                { "open", Val(meterPanelVisible) },
            });
        });
    }
}

QString ProjectPageModel::projectToolBarName() const
{
    return PROJECT_TOOLBAR_NAME;
}

QString ProjectPageModel::playbackToolBarName() const
{
    return PLAYBACK_TOOLBAR_NAME;
}

QString ProjectPageModel::undoRedoToolBarName() const
{
    return UNDO_REDO_TOOLBAR_NAME;
}

QString ProjectPageModel::workspacesToolBarName() const
{
    return WORKSPACES_TOOLBAR_NAME;
}

QString ProjectPageModel::tracksPanelName() const
{
    return TRACKS_PANEL_NAME;
}

QString ProjectPageModel::historyPanelName() const
{
    return HISTORY_PANEL_NAME;
}

QString ProjectPageModel::playbackMeterPanelName() const
{
    return PLAYBACK_METER_PANEL_NAME;
}

QString ProjectPageModel::statusBarName() const
{
    return PROJECT_STATUSBAR_NAME;
}

void ProjectPageModel::toggleDock(const QString& name)
{
    commandDispatcher()->dispatch(dock::DOCK_TOGGLE_COMMAND, { { "dock_name", Val(name.toStdString()) } });
}
