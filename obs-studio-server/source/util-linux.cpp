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

// Linux implementation of the platform helper that macOS implements in
// util-osx-impl.mm. The interface (util-osx.hpp) is plain C++.

#include "util-osx.hpp"

#include <obs.h>
#include <obs-nix-platform.h>
#include <util/platform.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>

#include <limits.h>
#include <pwd.h>
#include <unistd.h>

namespace {
enum class State { Starting, InitializedApi, InitializedCEF };

std::mutex g_mutex;
std::condition_variable g_cv;
bool g_running = false;
State g_state = State::Starting;
Display *g_x11_display = nullptr;

std::string home_directory()
{
	if (const char *home = getenv("HOME"))
		return home;
	if (const passwd *pw = getpwuid(getuid()))
		return pw->pw_dir;
	return "/tmp";
}

std::string xdg_user_dir(const char *name, const char *fallback)
{
	// Parse ~/.config/user-dirs.dirs, e.g. XDG_VIDEOS_DIR="$HOME/Videos"
	const std::string home = home_directory();
	const char *configHome = getenv("XDG_CONFIG_HOME");
	const std::string configDir = configHome && *configHome ? configHome : home + "/.config";

	std::ifstream file(configDir + "/user-dirs.dirs");
	std::string line;
	const std::string prefix = std::string(name) + "=\"";

	while (std::getline(file, line)) {
		if (line.rfind(prefix, 0) != 0 || line.back() != '"')
			continue;

		std::string value = line.substr(prefix.size(), line.size() - prefix.size() - 1);
		if (value.rfind("$HOME", 0) == 0)
			value = home + value.substr(5);
		return value;
	}

	return home + "/" + fallback;
}
} // namespace

UtilInt::UtilInt(void) : _impl(nullptr) {}

UtilInt::~UtilInt(void)
{
	if (g_x11_display) {
		XCloseDisplay(g_x11_display);
		g_x11_display = nullptr;
	}
}

void UtilInt::init(void)
{
	// libobs needs to know the windowing system before obs_startup(). The
	// Electron frontend of the Linux port runs on X11 (XWayland on Wayland
	// desktops), so the preview windows are X11 windows rendered with EGL.
	XInitThreads();
	g_x11_display = XOpenDisplay(nullptr);
	if (g_x11_display) {
		obs_set_nix_platform(OBS_NIX_PLATFORM_X11_EGL);
		obs_set_nix_platform_display(g_x11_display);
	} else {
		blog(LOG_ERROR, "Unable to open the X11 display, previews will not work");
	}
}

std::string UtilInt::getDefaultVideoSavePath(void)
{
	return xdg_user_dir("XDG_VIDEOS_DIR", "Videos");
}

void UtilInt::runApplication(void)
{
	// macOS runs the Cocoa event loop here. Linux has no main-thread event
	// loop to run, so simply block until the server is asked to stop.
	std::unique_lock<std::mutex> lock(g_mutex);
	g_running = true;
	g_cv.wait(lock, [] { return !g_running; });
}

void UtilInt::stopApplication(void)
{
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_running = false;
	}
	g_cv.notify_all();
}

bool UtilInt::isRunning(void)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_running;
}

unsigned long long UtilInt::getTotalPhysicalMemory(void)
{
	return os_get_sys_total_size();
}

unsigned long long UtilInt::getAvailableMemory(void)
{
	return os_get_sys_free_size();
}

std::vector<std::pair<uint32_t, uint32_t>> UtilInt::getAvailableScreenResolutions(void)
{
	std::vector<std::pair<uint32_t, uint32_t>> resolutions;
	if (!g_x11_display)
		return resolutions;

	const Window root = DefaultRootWindow(g_x11_display);
	XRRScreenResources *res = XRRGetScreenResourcesCurrent(g_x11_display, root);
	if (res) {
		for (int i = 0; i < res->ncrtc; i++) {
			XRRCrtcInfo *crtc = XRRGetCrtcInfo(g_x11_display, res, res->crtcs[i]);
			if (!crtc)
				continue;
			if (crtc->mode != 0 && crtc->width > 0 && crtc->height > 0)
				resolutions.emplace_back(crtc->width, crtc->height);
			XRRFreeCrtcInfo(crtc);
		}
		XRRFreeScreenResources(res);
	}

	if (resolutions.empty()) {
		const int screen = DefaultScreen(g_x11_display);
		resolutions.emplace_back(DisplayWidth(g_x11_display, screen), DisplayHeight(g_x11_display, screen));
	}

	return resolutions;
}

std::string UtilInt::getUserDataPath(void)
{
	const char *dataHome = getenv("XDG_CONFIG_HOME");
	return dataHome && *dataHome ? dataHome : home_directory() + "/.config";
}

std::string UtilInt::getWorkingDirectory(void)
{
	char cwd[PATH_MAX];
	if (getcwd(cwd, sizeof(cwd)))
		return cwd;
	return {};
}

std::string UtilInt::getComputerName(void)
{
	char name[HOST_NAME_MAX + 1] = {};
	if (gethostname(name, sizeof(name) - 1) == 0)
		return name;
	return {};
}

int UtilInt::getPhysicalCores(void)
{
	return os_get_physical_cores();
}

std::string UtilInt::getCpuName(void)
{
	std::ifstream file("/proc/cpuinfo");
	std::string line;
	while (std::getline(file, line)) {
		if (line.rfind("model name", 0) != 0)
			continue;
		const auto pos = line.find(':');
		if (pos == std::string::npos)
			continue;
		std::string name = line.substr(pos + 1);
		name.erase(0, name.find_first_not_of(" \t"));
		return name;
	}
	return {};
}

void UtilInt::nextState(void)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	if (g_state == State::Starting)
		g_state = State::InitializedApi;
	else if (g_state == State::InitializedApi)
		g_state = State::InitializedCEF;
}

bool UtilInt::hasInitApi(void)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_state == State::InitializedApi;
}

bool UtilInt::hasInitCef(void)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_state == State::InitializedCEF;
}
