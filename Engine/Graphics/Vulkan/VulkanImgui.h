#pragma once

#include <vector>

#include "ImGui/imgui.h"
#include "ImGui/Graphics/imgui_impl_glfw.h"
#include "ImGui/Graphics/imgui_impl_vulkan.h"

#include "VulkanObjects.h"

namespace Mega
{
	class Vulkan;
	
	class ImguiObject {
	public:
		friend Vulkan;
	
		void Initialize(GLFWwindow* in_pWindow);
		void Destroy(Vulkan* v);
	
		void CreateRenderData(Vulkan* v);
		void RenderFrame(Vulkan* v, uint32_t m_frameNumber);
	
	private:
		void CreateDescriptorPool(Vulkan* v);
		void CreateCommandData(Vulkan* v);
		void CreateFrameBuffers(Vulkan* v);
		void CreateRenderPass(Vulkan* v);
	
		GLFWwindow* m_pWindow = nullptr;
	
		VkDescriptorPool m_descriptorPool;
	
		VkRenderPass m_renderPass;
		VkPipeline m_pipeline;
	
		VkShaderModule m_vertShaderModule;
		VkShaderModule m_fragShaderModule;
	
		std::vector<VkFramebuffer> m_framebuffers;
		std::vector<VkCommandBuffer> m_commandBuffers;
		VkCommandPool m_commandPool;
	};
}