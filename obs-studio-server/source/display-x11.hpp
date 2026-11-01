/******************************************************************************
    Copyright (C) 2016-2019 by Streamlabs (General Workings Inc)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

******************************************************************************/

#pragma once

// Small X11 helper for the Linux preview displays. Like the Windows
// implementation, each display owns a child window inside the Electron
// window; libobs then creates its EGL surface inside that child window.
// Xlib headers are deliberately not included here because their macros
// (None, Status, Success, ...) clash with names used throughout OSN.

#include <cstdint>

namespace osn::x11 {

struct ChildWindow {
	void *display = nullptr;  // Display*
	unsigned long window = 0; // Window (XID)
};

bool CreateChildWindow(uint64_t parent, uint32_t width, uint32_t height, ChildWindow &out);
void DestroyChildWindow(ChildWindow &window);
void MoveResizeChildWindow(const ChildWindow &window, int x, int y, uint32_t width, uint32_t height);
void SetChildWindowStacking(const ChildWindow &window, bool bottom);

} // namespace osn::x11
