#include "ShaderCompiler.h"

#include "Engine/Core/Debug.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <windows.h>
#include <stdlib.h>

namespace Mega
{
	VkShaderModule ShaderHelper::CompileAndCreateShader(const VkDevice in_device, const std::string& in_inputShaderPath, const std::string& in_outputShaderPath)
	{
		if (!std::filesystem::exists(in_inputShaderPath)) { std::cout << "Failed to open shader file" << std::endl; MEGA_RUNTIME_ERROR("Failed to open shader file"); }
//#ifdef _DEBUG // Only compile shaders in debug mode
		
			std::cout << "Compiling Shader from " << in_inputShaderPath << " to " << in_outputShaderPath << std::endl;
			
			std::string exePath = """middleware\\GLSL\\glslc.exe""";
			if (!std::filesystem::exists(exePath)) { std::cout << "Failed to open shader exe file" << std::endl; MEGA_RUNTIME_ERROR("Failed to open shader file"); }
			
			// Compile
			int result = system(std::string(exePath + " " + in_inputShaderPath + " -o " + in_outputShaderPath).c_str());
			if (result != 0) { std::cout << "Process executed with error " << result << std::endl; }
			MEGA_ASSERT(result == 0, "Shader did not compile correctly");
//#endif

		// Use the compiled shaders to create a vkShaderModule
		std::vector<char> byteCode = ReadFile(in_outputShaderPath.c_str());
		VkShaderModule out_shaderModuke = CreateShaderModule(in_device, byteCode);

		std::cout << "Shader Compiled!" << std::endl;
		return out_shaderModuke;
	}

	std::vector<char> ShaderHelper::ReadFile(const char* in_filePath)
	{
		// Reads all the bytes from the specified file and returns a vector of those bytes
		std::ifstream file(in_filePath, std::ios::ate | std::ios::binary); // Start reading at end and read in binary
		if (!file.is_open()) { std::cout << "Failed to open shader file" << std::endl; MEGA_RUNTIME_ERROR("Failed to open shader file"); }

		size_t fileSize = (size_t)file.tellg(); // Reading at the end to get size
		std::vector<char> out_buffer(fileSize);

		file.seekg(0); // Go back to the beginning and start reading
		file.read(out_buffer.data(), fileSize);

		file.close();

		return out_buffer;
	}
	VkShaderModule ShaderHelper::CreateShaderModule(const VkDevice in_device, const std::vector<char>& in_code)
	{
		// Creates a shader module, basically a wrapper for the byte code, from the specified bytes
		VkShaderModule out_shaderModule{};

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = in_code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(in_code.data());

		VkResult result = vkCreateShaderModule(in_device, &createInfo, nullptr, &out_shaderModule);
		assert(result == VK_SUCCESS && "ERROR: vkCreateShaderModule() did not return success");

		return out_shaderModule;
	}
}