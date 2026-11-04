/*
* Audacity: A Digital Audio Editor
*/
#ifndef AU_APPSHELL_PROJECTPAGEMODEL_H
#define AU_APPSHELL_PROJECTPAGEMODEL_H

#include "framework/global/modularity/ioc.h"
#include "framework/global/async/asyncable.h"
#include "framework/rcommand/icommanddispatcher.h"
#include "framework/dockwindow/docktypes.h"
#include "framework/dockwindow/idockwindowprovider.h"
#include "framework/ui/iuiconfiguration.h"
#include "framework/ui/iuistate.h"

#include "context/iglobalcontext.h"
#include "playback/iplaybackuistate.h"
#include "video/ivideoservice.h"
#include "appshell/iapplicationactioncontroller.h"

#include <QtQml/qqmlregistration.h>

namespace au::appshell {
class ProjectPageModel : public QObject, public muse::async::Asyncable, public muse::Contextable
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int videoPanelLocation READ videoPanelLocation NOTIFY videoPanelLocationChanged FINAL)

    muse::GlobalInject<muse::ui::IUiConfiguration> uiConfiguration;

    muse::ContextInject<muse::rcommand::ICommandDispatcher> commandDispatcher { this };
    muse::ContextInject<IApplicationActionController> applicationActionController { this };
    muse::ContextInject<au::context::IGlobalContext> globalContext { this };
    muse::ContextInject<muse::dock::IDockWindowProvider> dockWindowProvider { this };
    muse::ContextInject<muse::ui::IUiState> uiState { this };
    muse::ContextInject<playback::IPlaybackUiState> playbackUiState { this };
    muse::ContextInject<video::IVideoService> videoService { this };

public:
    explicit ProjectPageModel(QObject* parent = nullptr);

    Q_INVOKABLE void init();

    int videoPanelLocation() const;
    Q_INVOKABLE void moveVideoPanel(int location, int extent);
    Q_INVOKABLE void resizeVideoPanel(int extent);

    Q_INVOKABLE QString projectToolBarName() const;
    Q_INVOKABLE QString playbackToolBarName() const;
    Q_INVOKABLE QString undoRedoToolBarName() const;
    Q_INVOKABLE QString workspacesToolBarName() const;

    Q_INVOKABLE QString tracksPanelName() const;
    Q_INVOKABLE QString historyPanelName() const;
    Q_INVOKABLE QString playbackMeterPanelName() const;

    Q_INVOKABLE QString statusBarName() const;

signals:
    void videoPanelLocationChanged();
    void videoPanelSizeResetRequested();

private:
    void toggleDock(const QString& name);

    void updatePlaybackMeterVisibility();
    void updateVideoPanelVisibility();
    void updateVideoPanelLocation();
    void setVideoPanelLocation(int location);

    bool m_inited = false;
    int m_videoPanelLocation = muse::dock::Location::Right;
    bool m_videoPanelSizeResetPending = false;
};
}

#endif // AU_APPSHELL_PROJECTPAGEMODEL_H
