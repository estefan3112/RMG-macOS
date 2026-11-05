#include "librashader_filter.hpp"
#include "util/logging.hpp"

#define LIBRA_RUNTIME_VULKAN
#include "librashader/librashader.h"

#include <stdlib.h>
#include <string.h>
#include <vector>

#ifndef _WIN32
#include <dlfcn.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace ShaderFilter
{
struct Api
{
	void *handle = nullptr;
	PFN_libra_instance_abi_version abi_version = nullptr;
	PFN_libra_preset_create preset_create = nullptr;
	PFN_libra_preset_free preset_free = nullptr;
	PFN_libra_vk_filter_chain_create_deferred chain_create_deferred = nullptr;
	PFN_libra_vk_filter_chain_frame chain_frame = nullptr;
	PFN_libra_vk_filter_chain_free chain_free = nullptr;
	PFN_libra_error_print error_print = nullptr;
	PFN_libra_error_free error_free = nullptr;
};

static Api api;
static libra_vk_filter_chain_t chain = nullptr;
static Vulkan::ImageHandle output;
static size_t frame_count = 0;

static void report(const char *what, libra_error_t error)
{
	LOGE("librashader: %s failed\n", what);
	if (error && api.error_print)
		api.error_print(error);
	if (error && api.error_free)
		api.error_free(&error);
}

#ifndef _WIN32
static std::vector<std::string> library_candidates()
{
	std::vector<std::string> paths;
	if (const char *env = getenv("RMG_LIBRASHADER"))
		paths.push_back(env);
#ifdef __APPLE__
	// inside the app bundle: Contents/MacOS/RMG -> Contents/Frameworks
	char exe[4096];
	uint32_t size = sizeof(exe);
	if (_NSGetExecutablePath(exe, &size) == 0)
	{
		std::string dir(exe);
		dir = dir.substr(0, dir.find_last_of('/'));
		paths.push_back(dir + "/../Frameworks/librashader.dylib");
	}
	paths.push_back("/opt/homebrew/lib/librashader.dylib");
	paths.push_back("/usr/local/lib/librashader.dylib");
	paths.push_back("librashader.dylib");
#else
	paths.push_back("librashader.so");
#endif
	return paths;
}

template <typename T>
static bool load_symbol(T &target, const char *name)
{
	target = reinterpret_cast<T>(dlsym(api.handle, name));
	if (!target)
		LOGE("librashader: missing symbol %s\n", name);
	return target != nullptr;
}

static bool load_library()
{
	if (api.handle)
		return true;

	for (const auto &path : library_candidates())
	{
		api.handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
		if (api.handle)
		{
			LOGI("librashader: loaded %s\n", path.c_str());
			break;
		}
	}
	if (!api.handle)
	{
		LOGE("librashader: library not found, shaders are disabled\n");
		return false;
	}

	bool ok = load_symbol(api.abi_version, "libra_instance_abi_version") &&
	          load_symbol(api.preset_create, "libra_preset_create") &&
	          load_symbol(api.preset_free, "libra_preset_free") &&
	          load_symbol(api.chain_create_deferred, "libra_vk_filter_chain_create_deferred") &&
	          load_symbol(api.chain_frame, "libra_vk_filter_chain_frame") &&
	          load_symbol(api.chain_free, "libra_vk_filter_chain_free") &&
	          load_symbol(api.error_print, "libra_error_print") &&
	          load_symbol(api.error_free, "libra_error_free");

	if (ok && api.abi_version() != LIBRASHADER_CURRENT_ABI)
	{
		LOGE("librashader: ABI version %u is not supported (expected %u)\n",
		     unsigned(api.abi_version()), unsigned(LIBRASHADER_CURRENT_ABI));
		ok = false;
	}

	if (!ok)
	{
		dlclose(api.handle);
		api = {};
	}
	return ok;
}
#else
static bool load_library()
{
	return false;
}
#endif

bool init(Vulkan::Device &device, const std::string &preset_path)
{
	if (preset_path.empty() || !load_library())
		return false;

	libra_shader_preset_t preset = nullptr;
	libra_error_t error = api.preset_create(preset_path.c_str(), &preset);
	if (error)
	{
		report(("loading preset " + preset_path).c_str(), error);
		return false;
	}

	libra_device_vk_t vk = {};
	vk.physical_device = device.get_physical_device();
	vk.instance = device.get_instance();
	vk.device = device.get_device();
	vk.queue = device.get_queue_info().queues[Vulkan::QUEUE_INDEX_GRAPHICS];
	vk.entry = vkGetInstanceProcAddr;

	filter_chain_vk_opt_t options = {};
	options.version = LIBRASHADER_CURRENT_VERSION;
	options.frames_in_flight = 3;

	// the GPU side setup is recorded into our own command buffer,
	// so librashader never touches the queue behind Granite's back
	auto cmd = device.request_command_buffer();
	error = api.chain_create_deferred(&preset, vk, cmd->get_command_buffer(), &options, &chain);
	device.submit(cmd);
	device.wait_idle();

	if (error)
	{
		report("creating the filter chain", error);
		api.preset_free(&preset);
		chain = nullptr;
		return false;
	}

	frame_count = 0;
	LOGI("librashader: using shader preset %s\n", preset_path.c_str());
	return true;
}

bool active()
{
	return chain != nullptr;
}

Vulkan::ImageHandle apply(Vulkan::Device &device, const Vulkan::ImageHandle &input,
                          unsigned out_width, unsigned out_height)
{
	if (!chain || !input || out_width == 0 || out_height == 0)
		return input;

	if (!output || output->get_width() != out_width || output->get_height() != out_height)
	{
		auto info = Vulkan::ImageCreateInfo::render_target(out_width, out_height, VK_FORMAT_R8G8B8A8_UNORM);
		info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		info.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		output = device.create_image(info);
		if (!output)
		{
			LOGE("librashader: could not create the output image\n");
			return input;
		}
	}

	// the previous frame sampled the output image, its contents can be discarded
	auto cmd = device.request_command_buffer();
	cmd->image_barrier(*output, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 0,
	                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
	                   VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);
	device.submit(cmd);

	libra_image_vk_t in = {};
	in.handle = input->get_image();
	in.format = input->get_format();
	in.width = input->get_width();
	in.height = input->get_height();

	libra_image_vk_t out = {};
	out.handle = output->get_image();
	out.format = output->get_format();
	out.width = out_width;
	out.height = out_height;

	frame_vk_opt_t opt = {};
	opt.version = LIBRASHADER_CURRENT_VERSION;
	opt.frame_direction = 1;
	opt.total_subframes = 1;
	opt.current_subframe = 1;
	opt.frames_per_second = 60.0f;
	opt.brightness_nits = 200.0f;

	// librashader wants a command buffer without prior commands
	cmd = device.request_command_buffer();
	libra_error_t error = api.chain_frame(&chain, cmd->get_command_buffer(), frame_count++,
	                                      in, out, nullptr, nullptr, &opt);
	cmd->image_barrier(*output, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
	                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
	device.submit(cmd);

	if (error)
	{
		// don't spam the log every frame, switch the filter off instead
		report("rendering a frame", error);
		destroy(device);
		return input;
	}

	return output;
}

void destroy(Vulkan::Device &device)
{
	if (chain)
	{
		device.wait_idle();
		api.chain_free(&chain);
		chain = nullptr;
	}
	output.reset();
}
}
