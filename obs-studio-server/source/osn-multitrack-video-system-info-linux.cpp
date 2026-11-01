#include "osn-multitrack-video-system-info.hpp"

#include <util/platform.h>

#include <sys/utsname.h>

#include <fstream>
#include <string>

namespace osn {

static std::string read_os_release_value(const std::string &key)
{
	std::ifstream file("/etc/os-release");
	std::string line;
	const std::string prefix = key + "=";

	while (std::getline(file, line)) {
		if (line.rfind(prefix, 0) != 0)
			continue;

		std::string value = line.substr(prefix.size());
		if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
			value = value.substr(1, value.size() - 2);
		return value;
	}

	return {};
}

static std::optional<std::string> read_cpu_name()
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

	return std::nullopt;
}

static void fill_system_info(System &sysinfo)
{
	struct utsname uts = {};
	uname(&uts);

	const std::string pretty_name = read_os_release_value("PRETTY_NAME");

	sysinfo.name = "Linux";
	sysinfo.version = uts.release;
	sysinfo.release = pretty_name.empty() ? "unknown" : pretty_name;
	sysinfo.revision = read_os_release_value("VERSION_ID");
	sysinfo.build = 0;
	sysinfo.bits = sizeof(void *) * 8;

	const std::string machine = uts.machine;
	sysinfo.arm = machine.rfind("aarch64", 0) == 0 || machine.rfind("arm", 0) == 0;
	sysinfo.armEmulation = false;
}

void system_info(Capabilities &capabilities)
{
	capabilities.cpu.name = read_cpu_name();
	capabilities.cpu.physical_cores = os_get_physical_cores();
	capabilities.cpu.logical_cores = os_get_logical_cores();

	capabilities.memory.total = os_get_sys_total_size();
	capabilities.memory.free = os_get_sys_free_size();

	// GPU enumeration on Linux (DRM/PCI) is not implemented yet. Leaving it
	// empty lets the multitrack video service fall back to its defaults.
	fill_system_info(capabilities.system);
}

} // namespace osn
