/*
* Audacity: A Digital Audio Editor
*/
#include "videopanellayout.h"

#include "kddockwidgets/src/KDDockWidgets.h"
#include "kddockwidgets/src/core/DockRegistry.h"
#include "kddockwidgets/src/core/DockWidget.h"
#include "kddockwidgets/src/core/MainWindow.h"

#include "appshell/appshelltypes.h"

using namespace muse::dock;

namespace {
using KDockWidget = KDDockWidgets::Core::DockWidget;

KDockWidget* dockByName(int iocContextId, const QString& name)
{
    return KDDockWidgets::DockRegistry::self(iocContextId)->dockByName(name);
}

KDockWidget* openVideoPanel(int iocContextId)
{
    KDockWidget* panel = dockByName(iocContextId, au::appshell::VIDEO_PANEL_NAME);
    return panel && panel->isOpen() ? panel : nullptr;
}

bool isSide(Location location)
{
    return location == Location::Left || location == Location::Right;
}

//! The dock to place the panel next to. The side panels and the tracks view have to stay aligned row by row,
//! so top/bottom must span them all: the page's edge holders (see DockPage.qml) are outside of that row.
QString anchorName(Location location)
{
    switch (location) {
    case Location::Left: return QStringLiteral("ProjectPage_panelsDockingHolderLeft");
    case Location::Top: return QStringLiteral("ProjectPage_panelsDockingHolderTop");
    case Location::Bottom: return QStringLiteral("ProjectPage_panelsDockingHolderBottom");
    //! Same place as the default layout: next to the tracks view, before the other right panels
    case Location::Right: return QStringLiteral("ProjectPage_central");
    case Location::Center:
    case Location::Undefined:
        break;
    }
    return QString();
}

KDDockWidgets::Location toKDLocation(Location location)
{
    switch (location) {
    case Location::Left: return KDDockWidgets::Location_OnLeft;
    case Location::Right: return KDDockWidgets::Location_OnRight;
    case Location::Top: return KDDockWidgets::Location_OnTop;
    case Location::Bottom: return KDDockWidgets::Location_OnBottom;
    case Location::Center:
    case Location::Undefined:
        break;
    }
    return KDDockWidgets::Location_None;
}
}

Location au::appshell::videopanellayout::currentLocation(int iocContextId)
{
    const KDockWidget* panel = openVideoPanel(iocContextId);
    const KDockWidget* central = dockByName(iocContextId, anchorName(Location::Right));
    if (!panel || !central) {
        return Location::Undefined;
    }

    const QRect panelRect = panel->groupGeometry();
    const QRect centralRect = central->groupGeometry();

    if (panelRect.right() <= centralRect.left()) {
        return Location::Left;
    }
    if (panelRect.left() >= centralRect.right()) {
        return Location::Right;
    }
    return panelRect.bottom() <= centralRect.top() ? Location::Top : Location::Bottom;
}

void au::appshell::videopanellayout::move(int iocContextId, Location location, int extent)
{
    KDockWidget* panel = openVideoPanel(iocContextId);
    KDockWidget* anchor = dockByName(iocContextId, anchorName(location));
    KDDockWidgets::Core::MainWindow* window = panel ? panel->mainWindow() : nullptr;
    if (!window || !anchor) {
        return;
    }

    const QRect rect = panel->groupGeometry();
    const QSize size = isSide(location) ? QSize(extent, rect.height()) : QSize(rect.width(), extent);

    window->addDockWidget(panel, toKDLocation(location), anchor,
                          KDDockWidgets::InitialOption(KDDockWidgets::InitialVisibilityOption::StartVisible, size));

    resize(iocContextId, location, extent);
}

void au::appshell::videopanellayout::resize(int iocContextId, Location location, int extent)
{
    KDockWidget* panel = openVideoPanel(iocContextId);
    if (!panel) {
        return;
    }

    const QRect rect = panel->groupGeometry();
    const int delta = extent - (isSide(location) ? rect.width() : rect.height());

    //! The edge that faces the tracks view is the one that moves
    switch (location) {
    case Location::Left: panel->resizeInLayout(0, 0, delta, 0);
        break;
    case Location::Right: panel->resizeInLayout(delta, 0, 0, 0);
        break;
    case Location::Top: panel->resizeInLayout(0, 0, 0, delta);
        break;
    case Location::Bottom: panel->resizeInLayout(0, delta, 0, 0);
        break;
    case Location::Center:
    case Location::Undefined:
        break;
    }
}
