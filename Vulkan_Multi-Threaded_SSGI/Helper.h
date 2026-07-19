#pragma once
#include <array>
#include <string>
#include <vector>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vulkan/vulkan.h>

namespace Helper
{
	namespace Vulkan
	{
		int RunCommandCaptureOutput(const std::string& _cmd, std::string& _outStdout);

		namespace Shader
		{
			bool LoadShaderModule(VkDevice _device, const std::filesystem::path& _spvPath, VkShaderModule& _outModule);
		}
	}
}