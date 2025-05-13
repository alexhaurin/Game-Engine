#include "VulkanImgui.h"
#include "Vulkan.h"
#include "Engine/Core/Debug.h"

#define CHECK_VULKAN_RESULT(result, msg) if (true) { std::cout << "Vulkan error at " << msg << " with code " << result << std::endl; }

namespace Mega
{
	void ImguiObject::Initialize(GLFWwindow* in_pWindow)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		ImGui::StyleColorsDark();

		MEGA_ASSERT(in_pWindow != nullptr, "Window is null");
		m_pWindow = in_pWindow;
		ImGui_ImplGlfw_InitForVulkan(in_pWindow, true);
	}

	void ImguiObject::Destroy(Vulkan* v)
	{

	}

	void ImguiObject::CreateRenderData(Vulkan* v)
	{
		// Create necessary Vulkan objects
		CreateDescriptorPool(v);
		CreateCommandData(v);
		CreateRenderPass(v);
		CreateFrameBuffers(v);

		// Setup Platform/Renderer bindings
		ImGui_ImplVulkan_InitInfo initInfo{};
		initInfo.Instance = v->m_instance;
		initInfo.PhysicalDevice = v->m_physicalDevice;
		initInfo.Device = v->m_device;
		initInfo.QueueFamily = v->m_queueFamilyIndices.graphicsAndComputeFamily.value();
		initInfo.Queue = v->m_graphicsQueue;
		initInfo.PipelineCache = nullptr;
		initInfo.DescriptorPool = m_descriptorPool;
		initInfo.Allocator = nullptr;
		initInfo.MinImageCount = (uint32_t)v->m_swapchainImageViews.size();;
		initInfo.ImageCount = (uint32_t)v->m_swapchainImageViews.size();
		ImGui_ImplVulkan_Init(&initInfo, m_renderPass);

		// Upload Fonts
		VkCommandBuffer commandBuffer = v->BeginSingleTimeCommand(v->m_drawCommandPools[0]);
		ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
		v->EndSingleTimeCommand(v->m_drawCommandPools[0], commandBuffer);
	}

	void ImguiObject::RenderFrame(Vulkan* v, uint32_t in_frameNumber)
	{
		// Rebuild command buffers
		{
			auto err = vkResetCommandPool(v->m_device, m_commandPool, 0);
			VkCommandBufferBeginInfo info{};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			err = vkBeginCommandBuffer(m_commandBuffers[in_frameNumber], &info);
		}

		// Begine render pass
		VkClearValue clearValue{};
		clearValue.color = { 0, 0, 0 };
		{
			VkRenderPassBeginInfo info{};
			info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			info.renderPass = m_renderPass;
			info.framebuffer = m_framebuffers[in_frameNumber];
			info.renderArea.extent = v->m_swapchainExtent;
			info.clearValueCount = 1;
			info.pClearValues = &clearValue;
			vkCmdBeginRenderPass(m_commandBuffers[in_frameNumber], &info, VK_SUBPASS_CONTENTS_INLINE);
		}

		// Draw
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffers[in_frameNumber]);

		vkCmdEndRenderPass(m_commandBuffers[in_frameNumber]);
		vkEndCommandBuffer(m_commandBuffers[in_frameNumber]);
	}


	void ImguiObject::CreateDescriptorPool(Vulkan* v)
	{
		std::vector<VkDescriptorPoolSize> poolSizes = {
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(v->m_swapchainImages.size());

		VkResult result = vkCreateDescriptorPool(v->m_device, &poolInfo, nullptr, &m_descriptorPool);
		assert(result == VK_SUCCESS && "ERROR: ImGui vkCreateDescriptorPool() did not return success");
	}


	void ImguiObject::CreateCommandData(Vulkan* v)
	{
		// Command Pool
		VkCommandPoolCreateInfo commandPoolCreateInfo{};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.queueFamilyIndex = v->FindQueueFamilies(v->m_physicalDevice, v->m_surface).graphicsAndComputeFamily.value();
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(v->m_device, &commandPoolCreateInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
			throw std::runtime_error("Could not create graphics command pool");
		}

		// Command buffer
		m_commandBuffers.resize(v->m_swapchainImageViews.size());

		VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandPool = m_commandPool;
		commandBufferAllocateInfo.commandBufferCount = (uint32_t)v->m_swapchainImageViews.size();
		vkAllocateCommandBuffers(v->m_device, &commandBufferAllocateInfo, m_commandBuffers.data());
	}

	void ImguiObject::CreateRenderPass(Vulkan* v)
	{
		VkAttachmentDescription attachment{};
		attachment.format = v->m_surfaceFormat.format;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference color_attachment{};
		color_attachment.attachment = 0;
		color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;  // or VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		info.attachmentCount = 1;
		info.pAttachments = &attachment;
		info.subpassCount = 1;
		info.pSubpasses = &subpass;
		info.dependencyCount = 1;
		info.pDependencies = &dependency;
		if (vkCreateRenderPass(v->m_device, &info, nullptr, &m_renderPass) != VK_SUCCESS) {
			throw std::runtime_error("Could not create Dear ImGui's render pass");
		}
	}
	void ImguiObject::CreateFrameBuffers(Vulkan* v)
	{
		VkImageView attachment[1];
		m_framebuffers.resize(v->m_swapchainImageViews.size());

		VkFramebufferCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.renderPass = m_renderPass;
		info.attachmentCount = 1;
		info.pAttachments = attachment;
		info.height = v->m_swapchainExtent.height;
		info.width = v->m_swapchainExtent.width;
		info.layers = 1;
		for (uint32_t i = 0; i < v->m_swapchainImageViews.size(); i++)
		{
			attachment[0] = v->m_swapchainImageViews[i];
			vkCreateFramebuffer(v->m_device, &info, nullptr, &m_framebuffers[i]);
		}
	}
}