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

#include "display-x11.hpp"

#include <obs.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <algorithm>

namespace osn::x11 {

bool CreateChildWindow(uint64_t parent, uint32_t width, uint32_t height, ChildWindow &out)
{
	Display *display = XOpenDisplay(nullptr);
	if (!display) {
		blog(LOG_ERROR, "[DISPLAY] Unable to open X11 display");
		return false;
	}

	XSetWindowAttributes attributes = {};
	attributes.background_pixel = BlackPixel(display, DefaultScreen(display));
	// No input events are selected on this window, so X propagates mouse and
	// keyboard events to the Electron window it sits in.
	attributes.event_mask = NoEventMask;
	attributes.do_not_propagate_mask = NoEventMask;

	const Window window = XCreateWindow(display, static_cast<Window>(parent), 0, 0, std::max<uint32_t>(width, 1), std::max<uint32_t>(height, 1), 0,
					    CopyFromParent, InputOutput, CopyFromParent, CWBackPixel | CWEventMask | CWDontPropagate, &attributes);
	if (!window) {
		blog(LOG_ERROR, "[DISPLAY] Unable to create X11 child window in parent 0x%llx", static_cast<unsigned long long>(parent));
		XCloseDisplay(display);
		return false;
	}

	XMapWindow(display, window);
	XSync(display, False);

	out.display = display;
	out.window = window;
	return true;
}

void DestroyChildWindow(ChildWindow &window)
{
	Display *display = static_cast<Display *>(window.display);
	if (!display)
		return;

	if (window.window)
		XDestroyWindow(display, static_cast<Window>(window.window));
	XSync(display, False);
	XCloseDisplay(display);

	window.display = nullptr;
	window.window = 0;
}

void MoveResizeChildWindow(const ChildWindow &window, int x, int y, uint32_t width, uint32_t height)
{
	Display *display = static_cast<Display *>(window.display);
	if (!display || !window.window)
		return;

	if (width == 0 || height == 0) {
		XUnmapWindow(display, static_cast<Window>(window.window));
	} else {
		XMoveResizeWindow(display, static_cast<Window>(window.window), x, y, width, height);
		XMapWindow(display, static_cast<Window>(window.window));
	}
	XFlush(display);
}

void SetChildWindowStacking(const ChildWindow &window, bool bottom)
{
	Display *display = static_cast<Display *>(window.display);
	if (!display || !window.window)
		return;

	if (bottom)
		XLowerWindow(display, static_cast<Window>(window.window));
	else
		XRaiseWindow(display, static_cast<Window>(window.window));
	XFlush(display);
}

} // namespace osn::x11
