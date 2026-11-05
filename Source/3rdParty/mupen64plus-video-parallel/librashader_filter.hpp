#pragma once

#include "vulkan_headers.hpp"
#include "device.hpp"
#include <string>

// Optional post-processing of the paraLLEl-RDP output with RetroArch slang
// shader presets (.slangp), using librashader which is loaded at runtime.
namespace ShaderFilter
{
// Loads librashader and creates the filter chain for the given preset,
// returns false (and leaves the filter disabled) on any failure.
bool init(Vulkan::Device &device, const std::string &preset_path);

// Runs the filter chain on input (which must be in SHADER_READ_ONLY_OPTIMAL)
// and returns an image of out_width x out_height in SHADER_READ_ONLY_OPTIMAL,
// or input itself when the filter is disabled.
Vulkan::ImageHandle apply(Vulkan::Device &device, const Vulkan::ImageHandle &input,
                          unsigned out_width, unsigned out_height);

bool active();

// Must be called before the device is destroyed.
void destroy(Vulkan::Device &device);
}
