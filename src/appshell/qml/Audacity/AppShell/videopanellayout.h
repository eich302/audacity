/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "framework/dockwindow/docktypes.h"

//! NOTE: the dock framework has no API to re-dock a panel at runtime (only the user dragging it does that),
//! so these talk to KDDockWidgets directly. The panel keeps its declared location (`DockBase::location`),
//! because that one is also what a page reload places it at when the default layout is restored.
namespace au::appshell::videopanellayout {
//! Where the open panel currently is relative to the tracks view, Undefined if it isn't laid out
muse::dock::Location currentLocation(int iocContextId);

//! Re-docks the open panel. `extent` is its size across the new edge: the width on the sides, the height on top/bottom
void move(int iocContextId, muse::dock::Location location, int extent);

//! Resizes the open panel, docked at `location`, across that edge
void resize(int iocContextId, muse::dock::Location location, int extent);
}
