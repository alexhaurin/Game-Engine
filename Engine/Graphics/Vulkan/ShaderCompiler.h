#pragma once

#include <string>
#include "VulkanInclude.h"

namespace Mega
{
	namespace ShaderHelper
	{
		VkShaderModule CompileAndCreateShader(const VkDevice in_device, const std::string& in_inputShaderPath, const std::string& in_outputShaderPath);

		VkShaderModule CreateShaderModule(const VkDevice in_device, const std::vector<char>& in_code);
		std::vector<char> ReadFile(const char* in_filePath);
	}
}