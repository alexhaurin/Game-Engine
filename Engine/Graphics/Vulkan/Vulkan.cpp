#include "Vulkan.h"

#include <stdexcept>
#include <cassert>
#include <array>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

#include "VulkanDefines.h"
#include "ShaderCompiler.h"
#include "VulkanImgui.h"
#include "Engine/Scene/Scene.h"
#include "Engine/ECS/Components.h"
#include "Engine/Graphics/Objects/Objects.h"
#include "Engine/Graphics/RendererSystem.h"
#include "Engine/Wind/WindSystem.h"
#include "Engine/Engine.h"
#include "Engine/Animation/AnimationSystem.h"

#define STB_IMAGE_IMPLEMENTATION

#include <STB/stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <TinyObjLoader/tiny_obj_loader.h>

#ifdef NDEBUG
const bool g_enableValidationLayers = false;
const bool g_isDebugMode = false;
#else
const bool g_enableValidationLayers = true;
const bool g_isDebugMode = true;
#endif

#define VK_CHECK_RESULT(f)                                             \
{                                                                      \
    VkResult res = (f);                                                \
    if (res != VK_SUCCESS)                                             \
    {                                                                  \
		std::cout << "Vulkan Error: ";                                 \
        throw std::runtime_error("Vulkan API call failed with error code: " + std::to_string(res)); \
    }                                                                  \
}
#define CHECK_VULKAN_RESULT(result, msg) if (result != VK_SUCCESS) { std::cout << "Vulkan error at " << msg << " with code " << result << std::endl; }

// https://blogs.igalia.com/itoral/2017/07/30/working-with-lights-and-shadows-part-ii-the-shadow-map/
#define SHADOW_MAP_WIDTH 8192
#define SHADOW_MAP_HEIGHT 8192
#define SHADOW_MAP_NEAR 1.0f // Defined by g_near (not constant yet)
#define SHADOW_MAP_FAR 10.0f
#define SHADOW_MAP_FORMAT VK_FORMAT_D32_SFLOAT
#define SHADOW_MAP_DEPTH_BIAS_CONST 4.0f
#define SHADOW_MAP_DEPTH_BIAS_SLOPE 1.8f

#define GRASS_GLOBAL_DISPATCH 64
#define GRASS_LOCAL_DISPATCH 16 // Set in the grass shader
#define GRASS_MAX_INSTANCE_COUNT GRASS_GLOBAL_DISPATCH * GRASS_GLOBAL_DISPATCH * GRASS_LOCAL_DISPATCH * GRASS_LOCAL_DISPATCH
// TODO: grass max effecient?
#define GRASS_FLOWER_MAX_INSTANCE_COUNT GRASS_MAX_INSTANCE_COUNT

#define BLOOM_SAMPLE_COUNT 5
#define SAMPLE_COUNT VK_SAMPLE_COUNT_1_BIT

#define MAX_TEXTURE_COUNT 6

float g_r = 400;

Mega::VertexData g_fullScreenQuad;
Mega::VertexData g_grassBlade;
Mega::VertexData g_grassBladeLow;
Mega::VertexData g_flower;
Mega::TextureData g_noiseTexture;

glm::mat4 g_lightSpaceMat;
float g_near = -311.0f;
float g_far = 114.0f;
float g_size = 82.0f;
glm::vec3 g_clearColor = { 0.612, 0.704, 1.0 };
float g_grassDir = -1;
int g_grassBladeCount = 0;

// Grass compute SSBO
VkBuffer g_stagingBuffer;
VkDeviceMemory g_stagingBufferMemory;
void* g_mappedMemory;

namespace Mega
{
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto app = reinterpret_cast<Vulkan*>(glfwGetWindowUserPointer(window));
		app->m_frameBufferResized = true;
	}

	// ================================ Public Functions ============================= //
	void Vulkan::Initialize(RendererSystem* in_pRenderer, GLFWwindow* in_pWindow)
	{
		std::cout << "=============== Initializing Vulkan ==============\n" << std::endl;

		// Store a pointer to the main renderer for data transfering and window for rendering/setup
		assert(in_pRenderer != nullptr && "ERROR: Cannot pass nullptr in place of Renderer* in Vulkan::Initialize()");
		assert(in_pWindow != nullptr && "ERROR: Cannot pass nullptr in place of Window* in Vulkan::Initialize()");
		m_pRenderer = in_pRenderer;
		m_pWindow = in_pWindow;

		glfwSetWindowUserPointer(in_pWindow, this);
		glfwSetFramebufferSizeCallback(in_pWindow, framebufferResizeCallback);

		// Initialize Vulkan
		CreateInstance(); // Create and store instance

		CreateWin32SurfaceKHR(m_pWindow, m_surface); // Create and store the surface
		PickPhysicalDevice(m_physicalDevice); // Pick and store suitable GPU to use

		CreateLogicalDevice(m_device); // Create and store the logical device

		CreateSwapchain(m_pWindow, m_surface, m_swapchain);
		RetrieveSwapchainImages(m_swapchainImages, m_swapchain);
		CreateSwapchainImageViews(m_swapchainImageViews, m_swapchainImages);
		CreateSceneImages();

		CreateRenderPass();
		CreateDepthResources(m_depthObject);
		CreateColorResources(m_colorObject);
		CreateDrawCommandPools(m_drawCommandPools);
		CreateTextureSampler(m_sampler);

		LoadVertexData("Assets/Models/Shapes/FullScreenQuad.obj", &g_fullScreenQuad);
		LoadVertexData("Assets/Models/GrassBlade.obj", &g_grassBlade);
		LoadVertexData("Assets/Models/GrassBladeLow.obj", &g_grassBladeLow);
		LoadVertexData("Assets/Models/Flower.obj", &g_flower);

		VertexBuffer<Vertex>::Create(m_vertexBuffer, this);
		IndexBuffer::Create(m_indexBuffer, this);

		CreateDescriptorSetLayout(m_device, m_descriptorSetLayout);

		m_shadowMapPipeline = CreateShadowMapPipeline("Shaders/shadowMap", sizeof(ShadowMap::PushConstant));
		m_shadowMapAnimationPipeline = CreateShadowMapPipeline("Shaders/shadowMapAnimation", sizeof(ShadowMapAnimation::PushConstant), true);
		m_mainPipeline = CreateGraphicsPipeline(SHADER_PATH_VERT, SHADER_PATH_FRAG, sizeof(Model::PushConstant), "Main");
		m_animationPipeline = CreateGraphicsPipeline(SHADER_PATH_VERT_ANIMATION, SHADER_PATH_FRAG_ANIMATION, sizeof(AnimatedModel::PushConstant), "Animation", true);
		m_waterPipeline = CreateGraphicsPipeline(SHADER_PATH_VERT_WATER, SHADER_PATH_FRAG_WATER, sizeof(Water::PushConstant), "Water");
		m_grassPipeline = CreateGraphicsPipeline(SHADER_PATH_VERT_GRASS, SHADER_PATH_FRAG_GRASS, sizeof(Grass::PushConstant), "Grass", false, true);

		// Grass Compute Pipeline
		CreateComputePipeline("Shaders/grassCompute", &m_grassComputePipeline, 0);
		CreateFramebuffers(m_swapchainFramebuffers);
		CreateBloom();
		CreateDrawCommands(m_drawCommandBuffers, m_drawCommandPools);
		CreateUniformBuffers();

		for (int i = 0; i < m_swapchainImages.size(); i++)
		{
			ChangeImageLayout(m_swapchainImages[i], m_surfaceFormat.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		}

		// ================== Grass Compute ================ //
		CreateTextureImage(m_grassComputeTexture, "Assets/Textures/GrassTextureMap.png");
		CreateImageView(m_grassComputeTexture.view, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, m_grassComputeTexture.image);

		// Wind perlin noise
		CreateTextureImage(m_perlinNoiseTexture, "Assets/Textures/PerlinNoise.png");
		CreateImageView(m_perlinNoiseTexture.view, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, m_perlinNoiseTexture.image);
		// ================================================== //

		// =================== SSBO =============== //
		m_ssboAnimation.resize(m_swapchainImages.size());
		m_ssboAnimationMemory.resize(m_swapchainImages.size());

		m_animBufferSize = sizeof(glm::mat4) * MAX_BONE_COUNT;
		CreateBuffer(m_animBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, \
			m_animStagingBuffer, m_animStagingBufferMemory);
		vkMapMemory(m_device, m_animStagingBufferMemory, 0, m_animBufferSize, 0, &m_animStagingBufferMap);

		for (size_t i = 0; i < m_swapchainImages.size(); i++) {
			CreateBuffer(sizeof(glm::mat4) * MAX_BONE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, \
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_ssboAnimation[i], m_ssboAnimationMemory[i]);
			CopyBuffer(m_animStagingBuffer, m_ssboAnimation[i], m_animBufferSize);
		}
		// ======================================== //

		// ============== GRASS COMPUTE SSBO =========== //
		CreateBuffer(GRASS_MAX_INSTANCE_COUNT * sizeof(Grass::Instance), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, \
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_ssboGrassCompute, m_ssboGrassComputeMemory);

		CreateBuffer(GRASS_MAX_INSTANCE_COUNT * sizeof(Grass::Instance), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, \
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_ssboGrassLowCompute, m_ssboGrassLowComputeMemory);

		CreateBuffer(GRASS_FLOWER_MAX_INSTANCE_COUNT * sizeof(Grass::Instance), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, \
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_ssboGrassFlowerCompute, m_ssboGrassFlowerComputeMemory);


		// =================== WIND DATA SSBO ================= //
		m_ssboWindData.resize(m_swapchainImages.size());
		m_ssboWindDataMemory.resize(m_swapchainImages.size());

		m_windDataBufferSize = sizeof(glm::vec4) * WIND_SIM_GRID_DIMENSIONS_X * WIND_SIM_GRID_DIMENSIONS_Y + sizeof(glm::vec4); // plus global wind vector
		CreateBuffer(m_windDataBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, \
			m_windDataStagingBuffer, m_windDataStagingBufferMemory);

		vkMapMemory(m_device, m_windDataStagingBufferMemory, 0, m_windDataBufferSize, 0, &m_windDataStagingBufferMap);

		for (size_t i = 0; i < m_swapchainImages.size(); i++) {
			CreateBuffer(m_windDataBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, \
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_ssboWindData[i], m_ssboWindDataMemory[i]);
			CopyBuffer(m_windDataStagingBuffer, m_ssboWindData[i], m_windDataBufferSize); // TODO: do i need this?
		}

		// ============================================ //

		CreateDescriptorPool();
		CreateDescriptorSets();

		LoadTextureData("Assets/Textures/PerlinNoise.png", &g_noiseTexture);

		CreateSyncObjects();

		m_imguiObject.Initialize(m_pWindow);
		m_imguiObject.CreateRenderData(this);

		// =================== OPTIMIZATION OCTOBER ============================ //
		// uint32_t queryCount = 5;
		// 
		// VkQueryPoolCreateInfo queryCreateInfo{};
		// queryCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		// queryCreateInfo.queryCount = queryCount;
		// 
		// vkCreateQueryPool(m_device, &queryCreateInfo, nullptr, &m_queryPool);
		// ===================================================================== //

		// Create the staging buffer
		g_mappedMemory = malloc(4);
		CreateBuffer(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, \
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, g_stagingBuffer, g_stagingBufferMemory);

		// Map the staging buffer memory
		vkMapMemory(m_device, g_stagingBufferMemory, 0, 4, 0, &g_mappedMemory);

		m_startTime = Mega::Time();
	}
	void Vulkan::Destroy()
	{
		vkDeviceWaitIdle(m_device);

		// ============= ImGui ============= //
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		vkDestroyDescriptorPool(m_device, m_imguiDescriptorPool, nullptr);
		// ================================= //

		// Cleanup Vulkan
		CleanupSwapchain(&m_swapchain);

		vkDestroySampler(m_device, m_sampler, nullptr);
		for (auto& t : m_textures) { ImageObject::Destroy(&m_device, &t); }

		vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutAnimation, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutGrassCompute, nullptr);

		IndexBuffer::Destroy(m_indexBuffer);

		for (auto& pool : m_drawCommandPools) {
			vkDestroyCommandPool(m_device, pool, nullptr);
		}

		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
		}

		vkDestroyDevice(m_device, nullptr);
		vkDestroyInstance(m_instance, nullptr);

		delete m_pBoxVertexData;
	}
	void Vulkan::DestroyPipeline(Pipeline& in_pipeline)
	{
		vkDestroyShaderModule(m_device, in_pipeline.vertexShader, nullptr);
		vkDestroyShaderModule(m_device, in_pipeline.fragmentShader, nullptr);
		vkDestroyPipeline(m_device, in_pipeline.graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(m_device, in_pipeline.layout, nullptr);
	}
	void Vulkan::CleanupSwapchain(VkSwapchainKHR* in_swapchain)
	{
		ImageObject::Destroy(&m_device, &m_shadowMapObject);
		ImageObject::Destroy(&m_device, &m_depthObject);
		ImageObject::Destroy(&m_device, &m_colorObject);

		for (size_t i = 0; i < m_swapchainFramebuffers.size(); i++) {
			vkDestroyFramebuffer(m_device, m_swapchainFramebuffers[i], nullptr);
		}

		vkDestroyShaderModule(m_device, m_shadowMapPipeline.vertexShader, nullptr);
		vkDestroyPipeline(m_device, m_shadowMapPipeline.graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(m_device, m_shadowMapPipeline.layout, nullptr);

		DestroyPipeline(m_mainPipeline);
		DestroyPipeline(m_animationPipeline);
		DestroyPipeline(m_waterPipeline);
		DestroyPipeline(m_grassPipeline);
		ComputePipeline::Destroy(this, m_grassComputePipeline);

		vkDestroyRenderPass(m_device, m_renderPass, nullptr);
		vkDestroyRenderPass(m_device, m_renderPassShadowMap, nullptr);
		vkDestroyRenderPass(m_device, m_renderPassPost, nullptr);

		DestroySceneImages();
		for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
			vkDestroyImageView(m_device, m_swapchainImageViews[i], nullptr);
		}

		vkUnmapMemory(m_device, m_animStagingBufferMemory);
		for (size_t i = 0; i < m_swapchainImages.size(); i++) {
			vkDestroyBuffer(m_device, m_uniformBuffersVert[i], nullptr);
			vkFreeMemory(m_device, m_uniformBuffersMemoryVert[i], nullptr);
			vkDestroyBuffer(m_device, m_uniformBuffersFrag[i], nullptr);
			vkFreeMemory(m_device, m_uniformBuffersMemoryFrag[i], nullptr);
			vkDestroyBuffer(m_device, m_ssboAnimation[i], nullptr);
			vkFreeMemory(m_device, m_ssboAnimationMemory[i], nullptr);
			vkDestroyBuffer(m_device, m_ssboWindData[i], nullptr);
			vkFreeMemory(m_device, m_ssboWindDataMemory[i], nullptr);
			vkDestroyBuffer(m_device, m_uboGrassCompute[i], nullptr);
			vkFreeMemory(m_device, m_uboGrassComputeMemory[i], nullptr);
		}

		// Bloom
		DestroyBloom();


		vkDestroyBuffer(m_device, m_ssboGrassCompute, nullptr);
		vkFreeMemory(m_device, m_ssboGrassComputeMemory, nullptr);
		vkDestroyBuffer(m_device, m_ssboGrassLowCompute, nullptr);
		vkFreeMemory(m_device, m_ssboGrassLowComputeMemory, nullptr);
		vkDestroyBuffer(m_device, m_ssboGrassFlowerCompute, nullptr);
		vkFreeMemory(m_device, m_ssboGrassFlowerComputeMemory, nullptr);

		// Animation SSBO Staging Buffer
		vkDestroyBuffer(m_device, m_animStagingBuffer, nullptr);
		vkFreeMemory(m_device, m_animStagingBufferMemory, nullptr);

		vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	}

	void Vulkan::Pipeline::Bind(Pipeline& in_pipeline, VkCommandBuffer* in_pCB, int in_imageIndex)
	{
		vkCmdBindPipeline(*in_pCB, VK_PIPELINE_BIND_POINT_GRAPHICS, in_pipeline.graphicsPipeline);
	};

	void Vulkan::DrawFrame(const Scene* in_pScene)
	{
		////////////////////////////////////////////////////////////////////////////////
		// Shadow for first directional light
		Vec3 lightDirection = { 0, 0, 0 };
		auto view = in_pScene->GetRegistry().view<const Component::Light>();
		for (const auto& [entity, l] : view.each())
		{
			if (l.lightData.type == eLightTypes::Directional)
			{
				lightDirection = l.lightData.direction;
				break;
			}
		}

		//ImGui::DragFloat("Size", &g_size);
		//ImGui::DragFloat("Far", &g_far);
		//ImGui::DragFloat("Near", &g_near);

		glm::mat4 lightView = glm::lookAt(lightDirection, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 light_projection = glm::ortho(-g_size, g_size, -g_size, g_size, g_near, g_far);

		g_lightSpaceMat = light_projection * lightView;
		////////////////////////////////////////////////////////////////////////////////

		const auto& viewModels = in_pScene->GetRegistry().view<const Component::Model, const Component::Transform>();
		const auto& viewAnimatedModels = in_pScene->GetRegistry().view<const Component::AnimatedModel, const Component::Transform>();
		const auto& viewWaterModels = in_pScene->GetRegistry().view<const Component::Water, const Component::Transform>();

		vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
		vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			int width = 0, height = 0;
			glfwGetFramebufferSize(m_pWindow, &width, &height);
			while (width == 0 || height == 0) {
				glfwGetFramebufferSize(m_pWindow, &width, &height);
				glfwWaitEvents();
			}
			//RecreateSwapchain();
		}
		else {
			assert((result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) && "ERROR: Failed to acquire swapchain image");
		}

		// Compute submission        
		VK_CHECK_RESULT(vkWaitForFences(m_device, 1, &m_computeInFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX));
		VK_CHECK_RESULT(vkResetFences(m_device, 1, &m_computeInFlightFences[m_currentFrame]));
		VK_CHECK_RESULT(vkResetCommandBuffer(m_grassComputeCommandBuffers[m_currentFrame], 0));

		ImGui::DragInt("High Def Grass Blade Count", &g_grassBladeCount, 0, GRASS_MAX_INSTANCE_COUNT);

		ImGui::DragFloat3("Clear Color", &g_clearColor.x, 0.001f);

		UpdateUniformBuffer(imageIndex, in_pScene);
		ImGui::Render();

		VkCommandBufferBeginInfo computeBeginInfo{};
		computeBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		auto* computeCommandBuffer = &m_grassComputeCommandBuffers[m_currentFrame];
		if (vkBeginCommandBuffer(*computeCommandBuffer, &computeBeginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording compute command buffer!");
		}

		(vkCmdBindPipeline(*computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_grassComputePipeline.pipeline));

		(vkCmdBindDescriptorSets(*computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_grassComputePipeline.layout, 0, 1, &m_descriptorSetsGrassCompute[m_currentFrame], 0, nullptr));
		(vkCmdBindDescriptorSets(*computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_grassComputePipeline.layout, 1, 1, &m_descriptorSetsWindData[m_currentFrame], 0, nullptr));

		// Dispatch the compute shader with the calculated workgroup count
		vkCmdDispatch(*computeCommandBuffer, GRASS_GLOBAL_DISPATCH, 1, GRASS_GLOBAL_DISPATCH);

		if (vkEndCommandBuffer(*computeCommandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to record compute command buffer!");
		}

		VkSubmitInfo computeSubmitInfo{};
		computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = computeCommandBuffer;
		computeSubmitInfo.signalSemaphoreCount = 1;
		computeSubmitInfo.pSignalSemaphores = &m_computeFinishedSemaphores[m_currentFrame];

		if (vkQueueSubmit(m_computeQueue, 1, &computeSubmitInfo, m_computeInFlightFences[m_currentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit compute command buffer!");
		};

		// =================================== //

		// Mark the image as now being in use by this frame
		m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];

		// ============== SHADOW MAPPING ===================== //
		{
			auto* commandBuffer = &m_shadowMapCommandBuffers[imageIndex];

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0; // Optional
			beginInfo.pInheritanceInfo = nullptr; // Optional

			result = vkBeginCommandBuffer(*commandBuffer, &beginInfo);
			assert(result == VK_SUCCESS && "vkBeginCommandBuffer() did not return success");

			// Just need to clear the depth buffer
			VkClearValue clearValue{};
			clearValue.depthStencil.depth = 1.0f;
			clearValue.depthStencil.stencil = 0;

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = m_renderPassShadowMap;
			renderPassInfo.framebuffer = m_shadowMapFramebuffers[m_currentFrame];
			renderPassInfo.renderArea.extent.width = SHADOW_MAP_WIDTH;
			renderPassInfo.renderArea.extent.height = SHADOW_MAP_HEIGHT;
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearValue;

			// NORMAL MODELS
			vkCmdBeginRenderPass(*commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			Pipeline::Bind(m_shadowMapPipeline, commandBuffer, imageIndex);
			IndexBuffer::Bind(m_indexBuffer, commandBuffer);
			VertexBuffer<Vertex>::Bind(m_vertexBuffer, commandBuffer);
			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowMapPipeline.layout, 0, 1, &m_descriptorSets[imageIndex], 0, nullptr);

			ShadowMap::PushConstant pushData;
			pushData.lightSpaceMat = g_lightSpaceMat;
			for (const auto& [entity, m, t] : viewModels.each())
			{
				// Push constants
				pushData.transform = t.GetTransform();

				// TODO: offset this so we dont update g_lightSpaceMat every time
				vkCmdPushConstants(*commandBuffer, m_shadowMapPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowMap::PushConstant), &pushData);

				// Draw
				uint32_t s = (uint32_t)m.vertexData.indices[0];
				uint32_t e = (uint32_t)m.vertexData.indices[1];

				vkCmdDrawIndexed(*commandBuffer, e - s, 1, s, 0, 0);
			}

			// ANIMATED MODELS
			if (viewAnimatedModels.size_hint() > 0)
			{
				Pipeline::Bind(m_shadowMapAnimationPipeline, commandBuffer, imageIndex);
				IndexBuffer::Bind(m_indexBuffer, commandBuffer);
				VertexBuffer<AnimatedVertex>::Bind(m_animatedVertexBuffer, commandBuffer); // TODO names things either all "animated" or "animation"
				vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowMapAnimationPipeline.layout, 1, 1, &m_descriptorSetsAnimation[imageIndex], 0, nullptr);

				ShadowMapAnimation::PushConstant pushData;
				pushData.lightSpaceMat = g_lightSpaceMat;
				for (const auto& [entity, a, t] : viewAnimatedModels.each())
				{
					// Push constants
					pushData.skinningMatsIndiceStart = a.mesh.skinningMatsIndiceStart;
					pushData.transform = t.GetTransform();

					vkCmdPushConstants(*commandBuffer, m_shadowMapAnimationPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowMapAnimation::PushConstant), &pushData);

					// Draw
					uint32_t s = a.mesh.vertexData.indices[0];
					uint32_t e = a.mesh.vertexData.indices[1];

					vkCmdDrawIndexed(*commandBuffer, e - s, 1, s, 0, 0);
				}
			}

			vkCmdEndRenderPass(*commandBuffer);
			VK_CHECK_RESULT(vkEndCommandBuffer(*commandBuffer));

			VkPipelineStageFlags imageAvailableStageFlag = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
			// TODO: semaphore abstraction class should contatin its corresponding stage flag

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &m_shadowMapFinishedSemaphores[m_currentFrame];
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = commandBuffer;
			submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrame];
			submitInfo.pWaitDstStageMask = &imageAvailableStageFlag;
			submitInfo.waitSemaphoreCount = 1;

			VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
		}
		// =================================================== //

		// ======================= Draw =============== //
		auto* commandBuffer = &m_drawCommandBuffers[imageIndex];

		// Multiple colors for the depth and image attachment
		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { { g_clearColor.x, g_clearColor.y, g_clearColor.z } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		result = vkBeginCommandBuffer(*commandBuffer, &beginInfo);
		assert(result == VK_SUCCESS && "vkBeginCommandBuffer() did not return success");

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_renderPass;
		renderPassInfo.framebuffer = m_swapchainFramebuffers[imageIndex]; // framebuffers reference VkImageView objects which represent the attachments 
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_swapchainExtent;
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(*commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// ==================== Draw our models  ================== //
		IndexBuffer::Bind(m_indexBuffer, commandBuffer);
		VertexBuffer<Vertex>::Bind(m_vertexBuffer, commandBuffer);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_grassPipeline.layout, 0, 1, &m_descriptorSets[imageIndex], 0, nullptr);

		// ==================== Grass ============================= //
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_grassPipeline.layout, 1, 1, &m_descriptorSetsGrassCompute[imageIndex], 0, nullptr);
		{
			Grass::PushConstant grassPushData{};

			// ================ High level of detail grass ================== //
			{
				// PULL INSTANCE COUNT
				uint32_t grassBladeRenderCount = 0;
				CopyBuffer(m_ssboGrassCompute, g_stagingBuffer, 4);

				uint32_t* ssboData = reinterpret_cast<uint32_t*>(g_mappedMemory);
				grassBladeRenderCount = ssboData[0]; // Access the first element of the UBO data
				g_grassBladeCount = grassBladeRenderCount;

				((uint32_t*)g_mappedMemory)[0] = 0; // Reset ssbo count
				CopyBuffer(g_stagingBuffer, m_ssboGrassCompute, 4);

				// DRAW
				if (grassBladeRenderCount > 0)
				{
					grassPushData.grassType = 0; // High poly grass
					vkCmdPushConstants(*commandBuffer, m_grassPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Grass::PushConstant), &grassPushData);
					Pipeline::Bind(m_grassPipeline, commandBuffer, imageIndex);

					uint32_t s = (uint32_t)g_grassBlade.indices[0];
					uint32_t e = (uint32_t)g_grassBlade.indices[1];
					vkCmdDrawIndexed(*commandBuffer, e - s, grassBladeRenderCount, s, 0, 0);
				}
			}

			// ======================= Flowers ====================== //
			{
				// PULL INSTANCE COUNT
				uint32_t flowerRenderCount = 0;
				CopyBuffer(m_ssboGrassFlowerCompute, g_stagingBuffer, 4);

				uint32_t* ssboData = reinterpret_cast<uint32_t*>(g_mappedMemory);
				flowerRenderCount = ssboData[0]; // Access the first element of the UBO data

				((uint32_t*)g_mappedMemory)[0] = 0; // Reset ssbo count
				CopyBuffer(g_stagingBuffer, m_ssboGrassFlowerCompute, 4);

				// DRAW
				if (flowerRenderCount > 0)
				{
					grassPushData.grassType = 2; // Flowers
					vkCmdPushConstants(*commandBuffer, m_grassPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Grass::PushConstant), &grassPushData);
					Pipeline::Bind(m_grassPipeline, commandBuffer, imageIndex);

					uint32_t s = (uint32_t)g_flower.indices[0];
					uint32_t e = (uint32_t)g_flower.indices[1];
					vkCmdDrawIndexed(*commandBuffer, e - s, flowerRenderCount, s, 0, 0);
				}
			}

			// ================= Low level of detail grass ================== //
			{
				// PULL INSTANCE COUNT
				uint32_t grassBladeRenderCountLow = 0;
				CopyBuffer(m_ssboGrassLowCompute, g_stagingBuffer, 4);

				uint32_t* ssboData = reinterpret_cast<uint32_t*>(g_mappedMemory);
				grassBladeRenderCountLow = ssboData[0]; // Access the first element of the UBO data

				((uint32_t*)g_mappedMemory)[0] = 0; // Reset ssbo count
				CopyBuffer(g_stagingBuffer, m_ssboGrassLowCompute, 4);

				// DRAW
				if (grassBladeRenderCountLow > 0)
				{
					grassPushData.grassType = 1; // Low poly grass
					vkCmdPushConstants(*commandBuffer, m_grassPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Grass::PushConstant), &grassPushData);
					Pipeline::Bind(m_grassPipeline, commandBuffer, imageIndex);

					uint32_t s = (uint32_t)g_grassBladeLow.indices[0];
					uint32_t e = (uint32_t)g_grassBladeLow.indices[1];
					vkCmdDrawIndexed(*commandBuffer, e - s, grassBladeRenderCountLow, s, 0, 0);
				}
			}
		}

		// ======================================================== //
		Pipeline::Bind(m_mainPipeline, commandBuffer, imageIndex);
		vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_mainPipeline.layout, 0, 1, &m_descriptorSets[imageIndex], 0, nullptr);

		Model::PushConstant pushData;
		for (const auto& [entity, m, t] : viewModels.each())
		{
			// Push constants
			pushData.transform = t.GetTransform();
			pushData.textureIndex = m.textureData.index;
			m.materialData.SetValues(&pushData.materialValues);

			vkCmdPushConstants(*commandBuffer, m_mainPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Model::PushConstant), &pushData);

			// Draw
			uint32_t s = (uint32_t)m.vertexData.indices[0];
			uint32_t e = (uint32_t)m.vertexData.indices[1];

			vkCmdDrawIndexed(*commandBuffer, e - s, 1, s, 0, 0);
		}

		// ========================= Water ======================================= //
		if (viewWaterModels.size_hint() > 0)
		{
			Pipeline::Bind(m_waterPipeline, commandBuffer, imageIndex);
			int currentRuntime = GetRuntime();
			for (const auto& [entity, w, t] : viewWaterModels.each())
			{
				// Push constants
				Water::PushConstant pushData;
				pushData.transform = t.GetTransform();
				pushData.textureIndex = w.textureData.index;
				pushData.time = currentRuntime;
				w.materialData.SetValues(&pushData.materialValues);

				vkCmdPushConstants(*commandBuffer, m_waterPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Water::PushConstant), &pushData);

				// Draw
				uint32_t s = w.vertexData.indices[0];
				uint32_t e = w.vertexData.indices[1];

				vkCmdDrawIndexed(*commandBuffer, e - s, 1, s, 0, 0);
			}
		}

		// ============= Draw Our Skeleton Animation Models ============== //
		if (viewAnimatedModels.size_hint() > 0)
		{
			std::vector<VkDescriptorSet> sets = { m_descriptorSets[imageIndex], m_descriptorSetsAnimation[imageIndex] };
			VertexBuffer<AnimatedVertex>::Bind(m_animatedVertexBuffer, commandBuffer);
			Pipeline::Bind(m_animationPipeline, commandBuffer, imageIndex);
			vkCmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_animationPipeline.layout, 0, (uint32_t)sets.size(), sets.data(), 0, nullptr);

			AnimatedModel::PushConstant pushData;
			for (const auto& [entity, a, t] : viewAnimatedModels.each())
			{
				// Push constants
				pushData.skinningMatsIndiceStart = a.mesh.skinningMatsIndiceStart;
				pushData.transform = t.GetTransform();
				pushData.textureIndex = a.mesh.textureData.index;
				a.mesh.materialData.SetValues(&pushData.materialValues);

				vkCmdPushConstants(*commandBuffer, m_animationPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(AnimatedModel::PushConstant), &pushData);

				// Draw
				uint32_t s = a.mesh.vertexData.indices[0];
				uint32_t e = a.mesh.vertexData.indices[1];

				vkCmdDrawIndexed(*commandBuffer, e - s, 1, s, 0, 0);
			}
		}
		// ==================================================================== //
		ChangeImageLayout(m_swapchainImages[m_currentFrame], m_surfaceFormat.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		vkCmdEndRenderPass(*commandBuffer);
		result = vkEndCommandBuffer(*commandBuffer);
		CHECK_VULKAN_RESULT(result, "vkEndCommandBuffer");

		// ===================== ImGui =========================== //
		// Rebuild command buffers
		{
			VkResult result;

			result = vkResetCommandPool(m_device, m_imguiObject.m_commandPool, 0);
			CHECK_VULKAN_RESULT(result, "vkResetCommandPool");
			VkCommandBufferBeginInfo info{};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			result = vkBeginCommandBuffer(m_imguiObject.m_commandBuffers[imageIndex], &info);
			CHECK_VULKAN_RESULT(result, "vkBeginCommandBuffer");

			// Begine render pass
			VkClearValue clearValueImgui{};
			clearValueImgui.color = { 0, 0, 0 };

			VkRenderPassBeginInfo infoImgui{};
			infoImgui.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			infoImgui.renderPass = m_imguiObject.m_renderPass;
			infoImgui.framebuffer = m_imguiObject.m_framebuffers[imageIndex];
			infoImgui.renderArea.extent = m_swapchainExtent;
			infoImgui.clearValueCount = 1;
			infoImgui.pClearValues = &clearValueImgui;
			vkCmdBeginRenderPass(m_imguiObject.m_commandBuffers[imageIndex], &infoImgui, VK_SUBPASS_CONTENTS_INLINE);

			// Draw
			ImDrawData* imguiDrawData = ImGui::GetDrawData();
			if (imguiDrawData)
				ImGui_ImplVulkan_RenderDrawData(imguiDrawData, m_imguiObject.m_commandBuffers[imageIndex], VK_NULL_HANDLE);

			vkCmdEndRenderPass(m_imguiObject.m_commandBuffers[imageIndex]);
			result = vkEndCommandBuffer(m_imguiObject.m_commandBuffers[imageIndex]);
			CHECK_VULKAN_RESULT(result, "vkEndCommandBuffer");
		}

		// ===================== Submit ============================ //

		std::vector<VkCommandBuffer> submitCommands = {
			*commandBuffer,
			m_imguiObject.m_commandBuffers[imageIndex]
		};

		VkSemaphore waitSemaphores[] =
		{
			m_computeFinishedSemaphores[m_currentFrame],
			m_shadowMapFinishedSemaphores[m_currentFrame]
		};
		VkPipelineStageFlags waitStages[] = {
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		};

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 2;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommands.size());
		submitInfo.pCommandBuffers = submitCommands.data();
		submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
		submitInfo.signalSemaphoreCount = 1;

		// to be in layout VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL--instead, current layout is VK_IMAGE_LAYOUT_PRESENT_SRC_KHR.
		result = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]);
		//std::cout << "Result: " << result << std::endl;
		assert(result == VK_SUCCESS && "ERROR: vkQueueSubmit() did not return succes");

		// ================== Bloom Compute Pass ==================== //
		{
			auto* computeCommandBuffer = &m_bloomComputeCommandBuffers[m_currentFrame];

			VK_CHECK_RESULT(vkBeginCommandBuffer(*computeCommandBuffer, &computeBeginInfo));

			// Using general for now for simplicity 
			//ChangeImageLayout(m_swapchainImages[m_currentFrame], m_surfaceFormat.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

			// Copy swapchain image to compute shader input image
			//VkImageBlit blitInfo{};
			//blitInfo.srcOffsets[0] = { (int)m_swapchainExtent.width, (int)m_swapchainExtent.height, 0 };
			//blitInfo.srcOffsets[1] = { (int)m_swapchainExtent.width, (int)m_swapchainExtent.height, 1 };
			//blitInfo.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			//blitInfo.srcSubresource.mipLevel = 0;
			//blitInfo.srcSubresource.baseArrayLayer = 0;
			//blitInfo.srcSubresource.layerCount = 1;
			//blitInfo.dstOffsets[0] = { (int)m_swapchainExtent.width, (int)m_swapchainExtent.height, 0 };
			//blitInfo.dstOffsets[1] = { (int)m_swapchainExtent.width, (int)m_swapchainExtent.height, 1 };
			//blitInfo.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			//blitInfo.dstSubresource.mipLevel = 0;
			//blitInfo.dstSubresource.baseArrayLayer = 0;
			//blitInfo.dstSubresource.layerCount = 1;
			//
			//vkCmdBlitImage(*computeCommandBuffer, m_swapchainImages[m_currentFrame], VK_IMAGE_LAYOUT_GENERAL, \
			//	m_bloomInput.image, VK_IMAGE_LAYOUT_GENERAL, 1, &blitInfo, VK_FILTER_LINEAR);
			//
			//VkImageMemoryBarrier barrier{};
			//barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			//barrier.image = m_bloomInput.image;
			//barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			//barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			//barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			//barrier.subresourceRange.baseArrayLayer = 0;
			//barrier.subresourceRange.layerCount = 1;
			//barrier.subresourceRange.levelCount = 1;
			//barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			//barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			//barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			//barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			//
			//vkCmdPipelineBarrier(*computeCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, \
			//	VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			// Run compute shader / do post processing

			//vkCmdBindPipeline(*computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_bloomComputePipeline.pipeline);
			//vkCmdBindDescriptorSets(*computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_bloomComputePipeline.layout, \
			//	0, 1, &m_descriptorSetsBloomCompute[m_currentFrame], 0, nullptr);
			//
			//vkCmdDispatch(*computeCommandBuffer, 256, 256, 1);

			//VkImageMemoryBarrier memoryBarrier{};
			//memoryBarrier.image = m_bloomInput.image;
			//memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			//memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			//memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			//memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;  // Access mask for the compute shader writes
			//memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;   // Access mask for subsequent transfer write
			//memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			//memoryBarrier.subresourceRange.baseArrayLayer = 0;
			//memoryBarrier.subresourceRange.layerCount = 1;
			//memoryBarrier.subresourceRange.levelCount = 1;
			//memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			//memoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			//vkCmdPipelineBarrier(*computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			//	0, nullptr, 0, nullptr, 1, &memoryBarrier);
			//
			//// Do I need a barrier here?
			//
			//// Copy image back to swapchain
			//blitInfo = {};
			//blitInfo.srcOffsets[0] = { (int)m_swapchainExtent.width, (int)m_swapchainExtent.height, 0 };
			//blitInfo.srcOffsets[1] = { (int)m_swapchainExtent.width, (int)m_swapchainExtent.height, 1 };
			//blitInfo.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			//blitInfo.srcSubresource.mipLevel = 0;
			//blitInfo.srcSubresource.baseArrayLayer = 0;
			//blitInfo.srcSubresource.layerCount = 1;
			//blitInfo.dstOffsets[0] = { (int)m_swapchainExtent.width, (int)m_swapchainExtent.height, 0 };
			//blitInfo.dstOffsets[1] = { (int)m_swapchainExtent.width, (int)m_swapchainExtent.height, 1 };
			//blitInfo.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			//blitInfo.dstSubresource.mipLevel = 0;
			//blitInfo.dstSubresource.baseArrayLayer = 0;
			//blitInfo.dstSubresource.layerCount = 1;
			//
			//vkCmdBlitImage(*computeCommandBuffer, m_bloomInput.image, VK_IMAGE_LAYOUT_GENERAL, \
			//	m_swapchainImages[m_currentFrame], VK_IMAGE_LAYOUT_GENERAL, 1, &blitInfo, VK_FILTER_LINEAR);
			//
			//barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			//barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			//
			//barrier = {};
			//barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			//barrier.image = m_bloomInput.image;
			//barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			//barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			//barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			//barrier.subresourceRange.baseArrayLayer = 0;
			//barrier.subresourceRange.layerCount = 1;
			//barrier.subresourceRange.levelCount = 1;
			//barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			//barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			//
			//vkCmdPipelineBarrier(*computeCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, \
			//	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			// Submit to GPU
			VK_CHECK_RESULT(vkEndCommandBuffer(*computeCommandBuffer));

			VkPipelineStageFlags waitStages[] =
			{
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			};

			VkSubmitInfo computeSubmitInfo{};
			computeSubmitInfo.waitSemaphoreCount = 1;
			computeSubmitInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
			computeSubmitInfo.pWaitDstStageMask = waitStages;
			computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			computeSubmitInfo.commandBufferCount = 1;
			computeSubmitInfo.pCommandBuffers = computeCommandBuffer;
			computeSubmitInfo.signalSemaphoreCount = 1;
			computeSubmitInfo.pSignalSemaphores = &m_bloomFinishedSemaphores[m_currentFrame];

			VK_CHECK_RESULT(vkQueueSubmit(m_computeQueue, 1, &computeSubmitInfo, VK_NULL_HANDLE));
		}

		VkSwapchainKHR swapChains[] = { m_swapchain };

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_bloomFinishedSemaphores[m_currentFrame];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			int width = 0, height = 0;
			glfwGetFramebufferSize(m_pWindow, &width, &height);
			while (width == 0 || height == 0) {
				glfwGetFramebufferSize(m_pWindow, &width, &height);
				glfwWaitEvents();
			}
			//RecreateSwapchain();
		}
		else {
			assert((result == VK_SUCCESS) && "ERROR: Failed to acquire swapchain image");
		}

		m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	};

	void Vulkan::SetCamera(const EulerCamera* in_pCamera) {
		m_pCamera = in_pCamera;
	}
	void Vulkan::CreateTextureImage(ImageObject& in_imageObject, const char* in_filename)
	{
		// "Create a buffer in host visible memory so that we can use vkMapMemory and copy the pixels to it"
		int width, height, texChannels;
		stbi_uc* pixels = stbi_load(in_filename, &width, &height, &texChannels, STBI_rgb_alpha);

		if (!pixels) {
			std::cout << "Failed to load Texture: " << in_filename << std::endl;
			MEGA_RUNTIME_ERROR("ERROR: Failed to load texture image!");
		}

		VkDeviceSize imageSize = (VkDeviceSize)(width)*height * 4;
		in_imageObject.extent = glm::vec2(width, height);

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		// Copy the pixel data into the buffer
		void* data;
		vkMapMemory(m_device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(m_device, stagingBufferMemory);

		stbi_image_free(pixels);

		// Create the VkImage object
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<uint32_t>(width);
		imageInfo.extent.height = static_cast<uint32_t>(height);
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0; // Optional

		CreateImageObject(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, in_imageObject.image, in_imageObject.memory);

		// Copy the buffer data to the image object, after transitioning the layout, then transition it again for optimal shader reading
		ChangeImageLayout(in_imageObject.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		CopyBufferToImage(stagingBuffer, in_imageObject.image, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
		ChangeImageLayout(in_imageObject.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// Cleanup
		vkDestroyBuffer(m_device, stagingBuffer, nullptr);
		vkFreeMemory(m_device, stagingBufferMemory, nullptr);
	}
	void Vulkan::LoadTextureData(const char* in_texPath, TextureData* in_pTextureData) {
		uint32_t index = m_loadedTextureCount;

		if (m_loadedTextureCount >= MAX_TEXTURE_COUNT) {
			MEGA_ASSERT(false, "Too many textures loaded!");
			throw std::exception("ERROR: Max texture limit reached");
			while (index >= 10) { index = m_loadedTextureCount % 10; }
		}
		if (m_loadedTextureCount >= m_textures.size()) {
			ImageObject image;
			m_textures.push_back(image);
		}

		//  Create
		CreateTextureImage(m_textures[index], in_texPath);
		CreateImageView(m_textures[index].view, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, m_textures[index].image);
		++m_loadedTextureCount;

		// Update Loaded Textures
		for (size_t i = 0; i < m_swapchainImages.size(); i++)
		{
			std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = m_textures[index].view;
			imageInfo.sampler = m_sampler;

			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = m_descriptorSets[i];
			descriptorWrites[0].dstBinding = 2;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pImageInfo = &imageInfo;
			descriptorWrites[0].dstArrayElement = index;

			vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}

		// Output
		in_pTextureData->index = index;
		in_pTextureData->dimensions = m_textures[index].extent;
	}

	void Vulkan::LoadVertexData(const char* in_objPath, VertexData* in_pVertexData, const char* in_MTLDir)
	{
		// Loads and stores data into vertex and index buffer given a customobj file and
		// fills in_pVertexData with proper data to access the data stored in those buffers
		std::vector<INDEX_TYPE>& indices = m_indexBuffer.indices;
		std::vector<Vertex>& vertices = m_vertexBuffer.vertices;

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warning, error;

		bool result = tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, in_objPath, in_MTLDir);
		if (!result) {
			std::cout << "Failed to load OBJ" << std::endl;
			std::cout << error << std::endl;
			throw std::exception(error.c_str());
			return;
		};

		// Set vertex data indices start
		in_pVertexData->indices[0] = static_cast<uint32_t>(indices.size());

		// Fill it into are format
		uint32_t vertexCount = 0;
		std::unordered_map<Vertex, INDEX_TYPE> uniqueVertices;
		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex{};

				//shape.mesh.material_ids[]

				// Positon
				if (index.vertex_index >= 0) {
					int in = 3 * index.vertex_index;
					vertex.pos = {
						attrib.vertices[(size_t)in + 0],
						attrib.vertices[(size_t)in + 1],
						attrib.vertices[(size_t)in + 2]
					};

					in_pVertexData->max.x = max(in_pVertexData->max.x, vertex.pos.x);
					in_pVertexData->max.y = max(in_pVertexData->max.y, vertex.pos.y);
					in_pVertexData->max.z = max(in_pVertexData->max.z, vertex.pos.z);
				}

				// Texture Coordinate
				if (index.texcoord_index >= 0) {
					vertex.texCoord = {
						attrib.texcoords[(size_t)2 * index.texcoord_index + 0],
						1.0f - attrib.texcoords[(size_t)(2 * index.texcoord_index + 1)]
					};
				}

				// Normal
				if (index.normal_index >= 0) {
					float nx = attrib.normals[3 * index.normal_index + 0];
					float ny = attrib.normals[3 * index.normal_index + 1];
					float nz = attrib.normals[3 * index.normal_index + 2];

					vertex.normal = glm::normalize(glm::vec3(nx, ny, nz));
				}

				// Colors
				float cx = attrib.colors[3 * index.vertex_index + 0];
				float cy = attrib.colors[3 * index.vertex_index + 1];
				float cz = attrib.colors[3 * index.vertex_index + 2];

				vertex.color = glm::vec4(cx, cy, cz, 1.0f);

				// Unique Indices
				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = static_cast<INDEX_TYPE>(vertices.size());
					vertices.push_back(vertex);
					vertexCount++;
				}

				indices.push_back(uniqueVertices[vertex]);
			}
		}

		// Set the rest of the vertex data
		in_pVertexData->indices[1] = static_cast<uint32_t>(indices.size());
		in_pVertexData->pIndexData = indices.data();
		in_pVertexData->pVertexData = vertices.data();
		in_pVertexData->vertexCount = vertexCount;
	}

	void Vulkan::LoadOzzMeshData(const ozz::vector<ozz::sample::Mesh>& in_meshes, AnimatedVertexData* in_pVertexData)
	{
		std::cout << "Loading Ozz Mesh Data" << std::endl;

		std::vector<INDEX_TYPE>& indices = m_indexBuffer.indices;
		std::vector<AnimatedVertex>& vertices = m_animatedVertexBuffer.vertices;
		in_pVertexData->indices[0] = static_cast<uint32_t>(indices.size());
		size_t vertexCount = 0;
		uint32_t max = 0;

		// One ozz.mesh file is split into multiple different meshes, all with different parts
		// (one mesh is subdivided in sets of vertices with the same number of joint influences called parts)
		// so we must load the vertex data from each mesh and each part within each mesh and combine
		// it into one VertexData object for our renderer
		uint32_t c = 0;
		uint32_t jointIndiceOffset = 0;
		for (const ozz::sample::Mesh& currentMesh : in_meshes)
		{
			// The indices we load from the mesh.ozz file are local to each seperate mesh, but we need global indices
			// so we can load the data into our own index buffer which might already have indices in it, so we offset
			// each indice when we add it by the original number of vertices (must be reset for every mesh)
			INDEX_TYPE indexBufferOffset = static_cast<INDEX_TYPE>(vertices.size());
			for (size_t i = 0; i < currentMesh.parts.size(); ++i)
			{
				const ozz::sample::Mesh::Part& part = currentMesh.parts[i];
				// Create and fill a vertex with the position, normal, joint, etc data
				AnimatedVertex vert{};

				// Position/Normal/Color/UV data
				const float* positionsBegin = array_begin(part.positions);
				const float* normalsBegin = array_begin(part.normals);
				const float* uvsBegin = array_begin(part.uvs);
				const uint8_t* colorsBegin = array_begin(part.colors);
				const size_t   partVertexCount = part.vertex_count();

				// Joint indices data
				const uint32_t  influencesCount = part.influences_count();
				MEGA_ASSERT(influencesCount <= MAX_BONE_INFLUENCE, "Too many bone influences for this mesh");
				MEGA_ASSERT(influencesCount <= MAX_BONE_INFLUENCE, "Too many bone influences on a vertex");
				const size_t    jointIndicesCount = part.joint_indices.size();
				const uint16_t* jointIndicesBegin = array_begin(part.joint_indices);

				// Joint weight data
				const size_t jointWeightsCount = part.joint_weights.size();
				const float* jointWeightsBegin = array_begin(part.joint_weights);

				for (size_t j = 0; j < partVertexCount; j++)
				{
					// Where each corresponding data for the current vertex is in the ozz::Mesh::Part's buffer
					// arrayBegin + (j * the number of variables per vertex for that type (3 for positions and normal: x, y z, 4 for color: r, g, b, a, etc)
					const float* posIndex = positionsBegin + (j * ozz::sample::Mesh::Part::kPositionsCpnts);
					const float* normalIndex = normalsBegin + (j * ozz::sample::Mesh::Part::kPositionsCpnts);
					const float* uvIndex = uvsBegin + (j * ozz::sample::Mesh::Part::kUVsCpnts);
					const uint8_t* colorIndex = colorsBegin + (j * ozz::sample::Mesh::Part::kColorsCpnts);

					// Add to our normal vertex data
					vert.pos = { posIndex[0], posIndex[1], posIndex[2] };
					vert.normal = { normalIndex[0], normalIndex[1], normalIndex[2] };
					if (uvsBegin) { vert.texCoord = { uvIndex[0], uvIndex[1] }; } // Some will not have uvs or color data
					//if (colorsBegin) { vert.color    = { colorIndex[0], colorIndex[1], colorIndex[2], colorIndex[3] }; }

					// Add skeleton animation data
					// Each vertex should have a number of joint indices equal to partInfluences count,
					// so part.joint_indices.size() should be partInfluencesCount times bigger than part.vertex_count()
					MEGA_ASSERT((partVertexCount == (jointIndicesCount / influencesCount)), "Vertex and Joint index counts dont match up");

					// Add our joint indice data
					const uint16_t* jointIndicesIndex = jointIndicesBegin + (j * influencesCount); // From OzzMesh.h: Stride equals influences_count
					for (uint32_t h = 0; h < influencesCount; h++)
					{
						max = max(max, jointIndicesIndex[h] + jointIndiceOffset);
						vert.boneIDs[h] = jointIndicesIndex[h] + jointIndiceOffset;
					}

					// Add our joint weight data
					// The way the weights are set up in the mesh.ozz file is if there is only 1 influence, then we just assume vert.weight[0] = 1.0f,
					// if there are more than one bone influences, then we parse the first n-1 influences and store then total, and then the last one
					// of vert.weights[n - 1] is equal to (1 - total) because all weights for one vert should always equal 1
					float total = 0.0f;
					if (influencesCount > 1)
					{
						MEGA_ASSERT(jointWeightsBegin, "JointWeights begin pointer is NULL");
						MEGA_ASSERT((partVertexCount == (jointWeightsCount / (influencesCount - 1))), "Vertex and Joint weight counts dont match up");
						const float* jointWeightsIndex = jointWeightsBegin + (j * (influencesCount - 1)); // From OzzMesh.h: Stride equals influences_count - 1

						for (uint32_t h = 0; h < influencesCount - 1; h++)
						{
							MEGA_ASSERT(jointWeightsIndex[h] <= 1.0001 && jointWeightsIndex[h] >= -0.0001, "Weird joint weight value");
							if (!(jointWeightsIndex[h] <= 1 && jointWeightsIndex[h] >= 0))
							{
								std::cout << "Vert weght too big: " << jointWeightsIndex[h] << std::endl;
							}

							vert.weights[h] = jointWeightsIndex[h];
							total += jointWeightsIndex[h];
						}

						MEGA_ASSERT(total <= 1.0001, "Total vert weights to large");
						if (total > 1.0001) { std::cout << "Vert total big: " << total << std::endl; }
					}

					// Set the last vert weight, if influencesCount is 1, it should just be 1,
					// otherwise is should be the 1 - the total of all other weights for this vert
					vert.weights[influencesCount - 1] = 1.0f - total;

					// Push back
					vertexCount++;
					vertices.push_back(vert);
				}
			}

			std::wcout << "Max: " << max << std::endl;
			max = 0;

			// Now we add the indices to our index buffer, again offseting each one by the current (before loading thise mesh)
			// size of our vertex buffer
			const ozz::sample::Mesh::TriangleIndices& meshIndices = currentMesh.triangle_indices;
			const uint16_t* meshIndicesBegin = array_begin(meshIndices);
			for (int i = 0; i < currentMesh.triangle_index_count(); i++)
			{
				indices.push_back(meshIndicesBegin[i] + indexBufferOffset);
			}

			jointIndiceOffset += static_cast<uint32_t>(currentMesh.joint_remaps.size());
		}

		// Set the rest of the vertex data
		in_pVertexData->indices[1] = static_cast<uint32_t>(indices.size());
		in_pVertexData->pIndexData = indices.data();
		in_pVertexData->pVertexData = vertices.data();
		in_pVertexData->vertexCount = static_cast<uint32_t>(vertexCount);
	}

	// ================================ Private Functions ============================= //
	std::vector<const char*> GetRequiredExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (g_enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	void Vulkan::CreateInstance()
	{
		std::cout << " ----- Creating Vulkan Instance -----\n" << std::endl;

		bool result1 = CheckValidationLayerSupport();
		bool result2 = CheckGLFWExtensionSupport();
		MEGA_ASSERT(result1, "ERROR: A validation layer is not available");
		MEGA_ASSERT(result2, "ERROR: Not all of GLFW's required extensions are supported");

		VkApplicationInfo appInfo{}; // Zeroing the struct to make sure all values are set to 0 or null
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // Have to specify the type of struct
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (g_enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
			createInfo.ppEnabledLayerNames = m_validationLayers.data();
			createInfo.enabledLayerCount = 1;
		}
		else {
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
		assert(result == VK_SUCCESS && "ERROR: Vulkan vkCreateInstance() did not return success");
	}
	bool Vulkan::CheckValidationLayerSupport()
	{
		// Checks if all requested validation layers are available

		uint32_t vk_layerCount = 0;
		vkEnumerateInstanceLayerProperties(&vk_layerCount, nullptr); // First enumerate to get the number of layers

		if (vk_layerCount <= 0)
		{
			MEGA_WARNING_MSG("No validation layers could be found");
			return true;
		}

		std::vector<VkLayerProperties> vk_availableLayers(vk_layerCount);
		vkEnumerateInstanceLayerProperties(&vk_layerCount, vk_availableLayers.data()); // Enumerate again to add layers to vk_availableLayers

		// Layers manually added
		//for (auto& vk_layer : vk_availableLayers) { // Check if its in vk_availableLayers
		//	m_validationLayers.push_back(vk_layer.layerName);
		//}

		// Check to make sure every Layer we are adding is in vk_availableLayers
		std::vector<const char*> unfoundLayerNames;
		for (const char* addedLayer : m_validationLayers) { // For each validation layer in m_validationLayers;
			bool layerFound = false;

			for (auto& vk_layer : vk_availableLayers) { // Check if its in vk_availableLayers
				if (strcmp(vk_layer.layerName, addedLayer)) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) { // Add it to our list of unfound layers
				unfoundLayerNames.push_back(addedLayer);
			}
		}

		// Cout our data
		std::cout << "Enabled Vulkan Validation Layers:\n";
		for (const char* layer : m_validationLayers) {
			std::cout << layer << "\n";
		}

		if (unfoundLayerNames.size() > 0) {
			std::cout << "Unfound Validation Layers\n";
			for (auto& layer : unfoundLayerNames) {
				std::cout << layer << " not found" << "\n";
			}
			return false;
		}
		else {
			std::cout << "All required validation layers found\n";
			return true;
		}
	}
	bool Vulkan::CheckGLFWExtensionSupport()
	{
		// Checks if all of GLFW's required extensions are supported

		uint32_t vk_extensionCount = 0;
		uint32_t glfw_extensionCount = 0;
		const char** glfw_requiredExtensions;

		vkEnumerateInstanceExtensionProperties(nullptr, &vk_extensionCount, nullptr); // First enumerate to get the number of extensions
		std::vector<VkExtensionProperties> vk_availableExtensions(vk_extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &vk_extensionCount, vk_availableExtensions.data()); // Enumerate again to add extensions to vk_availableExtensions

		glfw_requiredExtensions = glfwGetRequiredInstanceExtensions(&glfw_extensionCount);

		// Check to make sure every Extension in glfwRequiredInstanceExtensions() is in our extension list
		std::vector<const char*> glfw_unfoundExtensions;
		for (uint32_t i = 0; i < glfw_extensionCount; i++) { // For each reuired GLFW extension...
			bool extensionFound = false;

			for (auto& vk_extension : vk_availableExtensions) { // Check if its in Vulkan's extension list
				if (strcmp(vk_extension.extensionName, *glfw_requiredExtensions)) {
					extensionFound = true;
					break;
				}
			}

			if (!extensionFound) { // Add it to our list of unfound required GLFW extensions
				glfw_unfoundExtensions.push_back(*glfw_requiredExtensions);
			}

			++glfw_requiredExtensions;
		}

		// Cout our data
		std::cout << "Found Vulkan Extensions:\n";
		for (auto& extension : vk_availableExtensions) {
			std::cout << extension.extensionName << ": Version " << extension.specVersion << "\n";
		}

		if (glfw_unfoundExtensions.size() > 0) {
			std::cout << "Unfound Required GLFW Extensions\n";
			for (auto& extension : glfw_unfoundExtensions) {
				std::cout << extension << " not found" << "\n";
			}
			return false;
		}
		else {
			std::cout << "All required GLFW extensions found\n";
			return true;
		}
	}

	void Vulkan::CreateWin32SurfaceKHR(GLFWwindow* in_pWindow, VkSurfaceKHR& in_surface)
	{
		std::cout << "Creating surface..." << std::endl;

		assert(in_pWindow != nullptr && "ERROR: Cannot create surface with null GLFW window");
		assert(m_instance != nullptr && "ERROR: Cannot create surface without a Vulkan instance");

		//int counter = 0;
		//HWND target = nullptr;
		//
		//for (HWND hwnd = GetTopWindow(NULL); hwnd != NULL; hwnd = GetNextWindow(hwnd, GW_HWNDNEXT))
		//{
		//
		//    if (!IsWindowVisible(hwnd)) { continue; }
		//
		//    const int length = GetWindowTextLength(hwnd);
		//    if (length == 0) { continue; }
		//
		//    wchar_t* title = new wchar_t[length + 1];
		//    GetWindowText(hwnd, title, length + 1);
		//
		//    ++counter;
		//    std::cout << counter << ": ";
		//    std::wcout << "HWND: " << hwnd << " Title: " << title << std::endl;
		//
		//    if (counter == 6) { target = hwnd; }
		//}

		//VkWin32SurfaceCreateInfoKHR createInfo{};
		//createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		//createInfo.hwnd = glfwGetWin32Window(in_pWindow);
		////createInfo.hwnd = target;
		//createInfo.hinstance = GetModuleHandle(nullptr);

		VkResult result = glfwCreateWindowSurface(m_instance, in_pWindow, nullptr, &in_surface);
		assert(result == VK_SUCCESS && "ERROR: vkCreateWin32SurfaceKHR() did not return success");
	}

	void Vulkan::PickPhysicalDevice(VkPhysicalDevice& in_device)
	{
		// Picks a suitable GPU to use

		std::cout << "Picking physical device..." << std::endl;

		assert(m_instance != nullptr && "ERROR: Cannot pick physical device using nullptr instance");
		assert(m_surface != nullptr && "ERROR: cannot pick physical device without a surface");

		uint32_t deviceCount = 0; // Doing the same thing where we enumerate each device and store the data
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
		assert(deviceCount > 0 && "ERROR: Could not find a GPU with Vulkan Support");

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

		for (const auto& device : devices) { // Go through each device we found and pick the best one
			std::cout << "Device: " << device << std::endl;
			std::cout << "Surface: " << m_surface << std::endl;
			if (IsPhysicalDeviceSuitable(device, m_surface)) { // For now, just pick the best one
				in_device = device;
				MEGA_ASSERT(SAMPLE_COUNT <= GetMaxUsableSampleCount(in_device), "Mip sample level too high");
				m_msaaSamples = SAMPLE_COUNT;
				break;
			}
		}

		assert(in_device != VK_NULL_HANDLE && "ERROR: Could not find a suitable GPU");
	}
	QueueFamilyIndices Vulkan::FindQueueFamilies(const VkPhysicalDevice in_device, VkSurfaceKHR in_surface)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(in_device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(in_device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
				indices.graphicsAndComputeFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(in_device, i, in_surface, &presentSupport);

			if (presentSupport) {
				indices.presentFamily = i;
			}

			if (indices.IsComplete()) {
				break;
			}

			i++;
		}

		return indices;
	}
	bool Vulkan::IsPhysicalDeviceSuitable(const VkPhysicalDevice& in_device, const VkSurfaceKHR& in_surface)
	{
		QueueFamilyIndices indices = FindQueueFamilies(in_device, in_surface);

		bool extensionsSupported = CheckPhysicalDeviceExtensionSupport(in_device);

		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(in_device, in_surface);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(in_device, &supportedFeatures);

		return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
	}
	bool Vulkan::CheckPhysicalDeviceExtensionSupport(const VkPhysicalDevice& in_device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(in_device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(in_device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(m_physicalDeviceExtensions.begin(), m_physicalDeviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}
	SwapChainSupportDetails Vulkan::QuerySwapChainSupport(const VkPhysicalDevice& in_device, const VkSurfaceKHR& in_surface)
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(in_device, in_surface, &details.surfaceCapabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(in_device, in_surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(in_device, in_surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(in_device, in_surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(in_device, in_surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}
	VkSampleCountFlagBits Vulkan::GetMaxUsableSampleCount(const VkPhysicalDevice& in_device) {
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(in_device, &physicalDeviceProperties);

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		if (counts & VK_SAMPLE_COUNT_64_BIT && MAX_MSAA_SAMPLE_COUNT >= 64) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT && MAX_MSAA_SAMPLE_COUNT >= 32) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT && MAX_MSAA_SAMPLE_COUNT >= 16) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT && MAX_MSAA_SAMPLE_COUNT >= 8) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT && MAX_MSAA_SAMPLE_COUNT >= 4) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT && MAX_MSAA_SAMPLE_COUNT >= 2) { return VK_SAMPLE_COUNT_2_BIT; }

		return VK_SAMPLE_COUNT_1_BIT;
	}

	void Vulkan::CreateLogicalDevice(VkDevice& in_device)
	{
		std::cout << "Creating logical device..." << std::endl;

		assert(m_physicalDevice != nullptr && "ERROR: Cannot create a Vulkan logical device without a physical device");

		m_queueFamilyIndices = Vulkan::FindQueueFamilies(m_physicalDevice, m_surface); // Fill in the queue family indices
		vkGetPhysicalDeviceProperties(m_physicalDevice, &m_physicalDeviceProperties); // Fill in the properties and features for later use
		vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_physicalDeviceFeatures);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos; // A list of VkDeviceQueueCreateInfo for each queue in m_queueFamilyIndices

		// Making sure both queueFamilyIndices have values before adding them
		std::set<uint32_t> uniqueQueueFamilies = { m_queueFamilyIndices.graphicsAndComputeFamily.value(), m_queueFamilyIndices.presentFamily.value() };
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};

			float queuePriority = 1.0f;
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;

			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		if (ENABLE_SAMPLE_SHADING) { deviceFeatures.sampleRateShading = VK_TRUE; }
		deviceFeatures.shaderStorageImageMultisample = VK_TRUE;
		deviceFeatures.shaderStorageImageExtendedFormats = VK_TRUE;

		// TODO: query physical device to make sure these things above are available

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(m_physicalDeviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = m_physicalDeviceExtensions.data();

		if (g_enableValidationLayers) { // Should this be here?
			deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
			deviceCreateInfo.ppEnabledLayerNames = m_validationLayers.data();
		}
		else {
			deviceCreateInfo.enabledLayerCount = 0;
		}

		VkResult result = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &in_device);
		assert(result == VK_SUCCESS && "ERROR: Vulkan vkCreateDevice() did not return a success");

		std::cout << "Logical device: " << in_device << std::endl;

		vkGetDeviceQueue(m_device, m_queueFamilyIndices.graphicsAndComputeFamily.value(), 0, &m_graphicsQueue); // Store the graphics queue of the device
		vkGetDeviceQueue(m_device, m_queueFamilyIndices.presentFamily.value(), 0, &m_presentQueue); // Store the present queue of the device
		vkGetDeviceQueue(m_device, m_queueFamilyIndices.graphicsAndComputeFamily.value(), 0, &m_computeQueue); // Store the present queue of the device
	}

	void Vulkan::CreateSwapchain(GLFWwindow* in_pWindow, const VkSurfaceKHR in_surface, VkSwapchainKHR& in_swapchain)
	{
		std::cout << "Creating Swapchain..." << std::endl;

		assert(m_physicalDevice != nullptr && "ERROR: Cannot create a swapchain with a null physical device");
		assert(m_device != nullptr && "ERROR: Cannot create a swapchain with a null logical device");

		assert(in_surface != nullptr && "ERROR: Cannot create a swapchain with a null surface");

		// Query the details of our swapchain given our chosen physical device and surface, because not eveything will be compatable with the surface etc
		SwapChainSupportDetails swapSupportDetails = Vulkan::QuerySwapChainSupport(m_physicalDevice, m_surface);

		// Choose the best ones out of our options
		std::cout << "SwapSupportDetails.formats size: " << swapSupportDetails.formats.size() << std::endl;
		m_surfaceFormat = Vulkan::ChooseSwapSurfaceFormat(swapSupportDetails.formats);
		m_presentMode = Vulkan::ChooseSwapPresentMode(swapSupportDetails.presentModes);
		m_swapchainExtent = Vulkan::ChooseSwapExtent(swapSupportDetails.surfaceCapabilities, in_pWindow); // Store the swapchain extent for later use

		uint32_t imageQueueCount = swapSupportDetails.surfaceCapabilities.minImageCount + 1; // Set number of images in queue
		uint32_t maxImages = swapSupportDetails.surfaceCapabilities.maxImageCount; // Cap it to the surface's maximum
		if (maxImages > 0 && imageQueueCount > maxImages) { imageQueueCount = maxImages; }

		VkSwapchainCreateInfoKHR createInfo{}; // Create info for swap chain
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = in_surface;
		createInfo.minImageCount = imageQueueCount;
		createInfo.imageFormat = m_surfaceFormat.format;
		createInfo.imageColorSpace = m_surfaceFormat.colorSpace;
		createInfo.imageExtent = m_swapchainExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // | VK_IMAGE_USAGE_STORAGE_BIT;

		QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice, in_surface);
		uint32_t queueFamilyIndices[] = { indices.graphicsAndComputeFamily.value(), indices.presentFamily.value() };

		if (indices.graphicsAndComputeFamily != indices.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Image is owned by one queue family at a time and ownership must be explicitly
																	 //  transferred before using it in another queue family, offers best performance
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		createInfo.preTransform = swapSupportDetails.surfaceCapabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = m_presentMode;
		createInfo.clipped = VK_TRUE; // Dont care about pixels that are obscured, like another window in front of it
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		VkResult result = vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &in_swapchain);
		assert(result == VK_SUCCESS && "ERROR: vkCreateSwapchainKHR() did not return success");
	}
	VkSurfaceFormatKHR Vulkan::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& in_availableFormats)
	{
		// Pick what surface format we want to use (color depth)

		assert(!in_availableFormats.empty() && "ERROR: Trying to chose a surface format from a vector of 0 choices in ChooseSwapSurfaceFormat()");

		for (const auto& availableFormat : in_availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB /*UNORM*/ && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
			std::cout << availableFormat.format << std::endl;
		}

		MEGA_ASSERT(false, "HDR format not supported");
		return in_availableFormats[0]; // Eventually make function that chooses best one
	}
	VkPresentModeKHR Vulkan::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& in_availableModes)
	{
		// Choose the conditions for showing images to the screen (probably most important)

		assert(!in_availableModes.empty() && "Trying to choose a present mode from a vector of 0 choices in ChooseSwapPresentMode()");

		for (const auto& presentMode : in_availableModes) {
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) { // Prefer VK_PRESENT_MODE_MAILBOX_KHR
				return presentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR; // Guaranteed to be available
	}
	VkExtent2D Vulkan::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& in_capabilities, GLFWwindow* in_pWindow)
	{
		// Choose VkExtent2D (resoluion of the swap chain images)
		// currentExtent (width/height of surface) is 0xFFFFFFFF if the "surface size [is] determined by the extent of a swapchain targeting the surface" 
		if (in_capabilities.currentExtent.width != UINT32_MAX) { return in_capabilities.currentExtent; }

		int width, height;
		glfwGetFramebufferSize(in_pWindow, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, in_capabilities.minImageExtent.width, in_capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, in_capabilities.minImageExtent.height, in_capabilities.maxImageExtent.height);

		return actualExtent;
	}

	void Vulkan::RetrieveSwapchainImages(std::vector<VkImage>& in_images, VkSwapchainKHR in_swapchain)
	{
		// Get a handle to the swapchain images in the form of VkImages

		uint32_t imageCount = 0;
		vkGetSwapchainImagesKHR(m_device, in_swapchain, &imageCount, nullptr);

		in_images.resize(imageCount);
		vkGetSwapchainImagesKHR(m_device, in_swapchain, &imageCount, in_images.data());
	}
	void Vulkan::CreateSwapchainImageViews(std::vector<VkImageView>& in_imageViews, const std::vector<VkImage>& in_images)
	{
		// To use a VkImage, we have to create a VkImageView for it, which is just a 'view' to that image
		// It describes how to access the image and which part of the image to access
		// "An image view is sufficient to start using an image as a texture, but it's not quite ready to be
		// used as a render target just yet. That requires one more step of indirection, known as a framebuffer"
		// This function creates and returns an ImageView for each image in the swapchain

		in_imageViews.resize(in_images.size());

		for (size_t i = 0; i < in_images.size(); i++) {
			VkImageViewCreateInfo viewCreateInfo{};
			viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewCreateInfo.image = in_images[i];
			viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewCreateInfo.format = m_surfaceFormat.format;

			viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // Equivalent to VK_COMPONENT_SWIZZLE_R
			viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY; // and so on
			viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewCreateInfo.subresourceRange.baseMipLevel = 0;
			viewCreateInfo.subresourceRange.levelCount = 1;
			viewCreateInfo.subresourceRange.baseArrayLayer = 0;
			viewCreateInfo.subresourceRange.layerCount = 1;

			VkResult result = vkCreateImageView(m_device, &viewCreateInfo, nullptr, &in_imageViews[i]);
			assert(result == VK_SUCCESS && "ERROR: vkCreateImageView() did not return sucess");
		}
	}

	void Vulkan::CreateDescriptorPool()
	{
		std::cout << "Creating Descriptor Pool..." << std::endl;

		std::array<VkDescriptorPoolSize, 13> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT);
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT);
		poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[2].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT) * MAX_TEXTURE_COUNT;

		// Shadow map
		poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[3].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT);

		// Skeleton Animation Mats SSBO
		poolSizes[4].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[4].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT);

		// Grass Compute SSBO
		poolSizes[5].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[5].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT);

		// Low LOD grass
		poolSizes[6].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[6].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT);

		// Flower
		poolSizes[7].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[7].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT);

		// Grass Compute UBO
		poolSizes[8].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[8].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT);

		// Bloom images
		poolSizes[9].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		poolSizes[9].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT * BLOOM_SAMPLE_COUNT + SWAPCHAIN_IMAGE_COUNT);

		// Grass Compute Textures
		poolSizes[10].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[10].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT);

		// Wind Da11 Buffers
		poolSizes[11].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[11].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT);

		// Wind perlin noise texture
		poolSizes[12].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[12].descriptorCount = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(SWAPCHAIN_IMAGE_COUNT * poolSizes.size()); // --------------- MUST CHANGE THIS TO ALLOCATE MORE POOL MEMORY

		VkResult result = vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool);
		assert(result == VK_SUCCESS && "ERROR: vkCreateDescriptorPool() did not return success");
	}

	void Vulkan::CreateDescriptorSetLayout(const VkDevice in_device, VkDescriptorSetLayout& in_descriptorSetLayout)
	{
		VkDescriptorSetLayoutBinding uboLayoutBindingVert{};
		uboLayoutBindingVert.binding = 0;
		uboLayoutBindingVert.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBindingVert.descriptorCount = 1;
		uboLayoutBindingVert.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutBinding uboLayoutBindingFrag{};
		uboLayoutBindingFrag.binding = 1;
		uboLayoutBindingFrag.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBindingFrag.descriptorCount = 1;
		uboLayoutBindingFrag.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 2;
		samplerLayoutBinding.descriptorCount = MAX_TEXTURE_COUNT;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutBinding shadowMapBinding{};
		shadowMapBinding.binding = 3;
		shadowMapBinding.descriptorCount = 1;
		shadowMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowMapBinding.pImmutableSamplers = nullptr;
		shadowMapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
			uboLayoutBindingVert,
			uboLayoutBindingFrag,
			samplerLayoutBinding,
			shadowMapBinding
		};
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		VkResult result = vkCreateDescriptorSetLayout(in_device, &layoutInfo, nullptr, &in_descriptorSetLayout); // tODO: no in_...
		assert(result == VK_SUCCESS && "ERROR: vkCreateDescriptorSetLayout() did not return success");

		// Create a descriptor set for the skeleton animation pipeline
		VkDescriptorSetLayoutBinding animationLayoutBinding{};
		animationLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		animationLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		animationLayoutBinding.binding = 0;
		animationLayoutBinding.descriptorCount = 1;

		std::array<VkDescriptorSetLayoutBinding, 1> animationBindings = {
			animationLayoutBinding
		};

		VkDescriptorSetLayoutCreateInfo animationLayoutInfo{};
		animationLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		animationLayoutInfo.bindingCount = static_cast<uint32_t>(animationBindings.size());
		animationLayoutInfo.pBindings = animationBindings.data();

		result = vkCreateDescriptorSetLayout(m_device, &animationLayoutInfo, nullptr, &m_descriptorSetLayoutAnimation);
		assert(result == VK_SUCCESS && "ERROR: vkCreateDescriptorSetLayout() did not return success");

		// Descriptor layout for Wind Data
		VkDescriptorSetLayoutBinding windDataLayoutBinding{};
		windDataLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		windDataLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		windDataLayoutBinding.binding = 0;
		windDataLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding perlinNoiseLayoutBinding{};
		perlinNoiseLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		perlinNoiseLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		perlinNoiseLayoutBinding.binding = 1;
		perlinNoiseLayoutBinding.descriptorCount = 1;

		std::array<VkDescriptorSetLayoutBinding, 2> windDataBindings = {
			windDataLayoutBinding,
			perlinNoiseLayoutBinding
		};

		VkDescriptorSetLayoutCreateInfo windDataLayoutInfo{};
		windDataLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		windDataLayoutInfo.bindingCount = static_cast<uint32_t>(windDataBindings.size());
		windDataLayoutInfo.pBindings = windDataBindings.data();

		result = vkCreateDescriptorSetLayout(m_device, &windDataLayoutInfo, nullptr, &m_descriptorSetLayoutWindData);
		assert(result == VK_SUCCESS && "ERROR: vkCreateDescriptorSetLayout() did not return success");

		// Grass Compute
		VkDescriptorSetLayoutBinding grassLayoutBindingSSBO{};
		grassLayoutBindingSSBO.binding = 0;
		grassLayoutBindingSSBO.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		grassLayoutBindingSSBO.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
		grassLayoutBindingSSBO.descriptorCount = 1;

		VkDescriptorSetLayoutBinding grassLowLayoutBindingSSBO{};
		grassLowLayoutBindingSSBO.binding = 1;
		grassLowLayoutBindingSSBO.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		grassLowLayoutBindingSSBO.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
		grassLowLayoutBindingSSBO.descriptorCount = 1;

		VkDescriptorSetLayoutBinding grassFlowerLayoutBindingSSBO{};
		grassFlowerLayoutBindingSSBO.binding = 2;
		grassFlowerLayoutBindingSSBO.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		grassFlowerLayoutBindingSSBO.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
		grassFlowerLayoutBindingSSBO.descriptorCount = 1;

		VkDescriptorSetLayoutBinding grassLayoutBindingUBO{};
		grassLayoutBindingUBO.binding = 3;
		grassLayoutBindingUBO.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		grassLayoutBindingUBO.descriptorCount = 1;
		grassLayoutBindingUBO.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorSetLayoutBinding grassLayoutBindingTexture{};
		grassLayoutBindingTexture.binding = 4;
		grassLayoutBindingTexture.descriptorCount = 1;
		grassLayoutBindingTexture.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		grassLayoutBindingTexture.pImmutableSamplers = nullptr;
		grassLayoutBindingTexture.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

		std::array<VkDescriptorSetLayoutBinding, 5> grassBindings = {
			grassLayoutBindingSSBO,
			grassLowLayoutBindingSSBO,
			grassFlowerLayoutBindingSSBO,
			grassLayoutBindingUBO,
			grassLayoutBindingTexture
		};

		VkDescriptorSetLayoutCreateInfo grassLayoutInfo{};
		grassLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		grassLayoutInfo.bindingCount = static_cast<uint32_t>(grassBindings.size());
		grassLayoutInfo.pBindings = grassBindings.data();

		result = vkCreateDescriptorSetLayout(m_device, &grassLayoutInfo, nullptr, &m_descriptorSetLayoutGrassCompute);
		assert(result == VK_SUCCESS && "ERROR: vkCreateDescriptorSetLayout() did not return success");

		// Bloom Compute
		{
			// Inputed frame to compute bloom from
			VkDescriptorSetLayoutBinding bloomImageBinding1{};
			bloomImageBinding1.binding = 0;
			bloomImageBinding1.descriptorCount = 1;
			bloomImageBinding1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			bloomImageBinding1.pImmutableSamplers = nullptr;
			bloomImageBinding1.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// Downsampled bloom image objects
			VkDescriptorSetLayoutBinding bloomImageBinding2{};
			bloomImageBinding2.binding = 1;
			bloomImageBinding2.descriptorCount = BLOOM_SAMPLE_COUNT;
			bloomImageBinding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			bloomImageBinding2.pImmutableSamplers = nullptr;
			bloomImageBinding2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			std::array<VkDescriptorSetLayoutBinding, 2> bloomBindings = {
				bloomImageBinding1,
				bloomImageBinding2
			};

			VkDescriptorSetLayoutCreateInfo bloomLayoutInfo{};
			bloomLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			bloomLayoutInfo.bindingCount = static_cast<uint32_t>(bloomBindings.size());
			bloomLayoutInfo.pBindings = bloomBindings.data();

			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &bloomLayoutInfo, nullptr, &m_descriptorSetLayoutBloomCompute));
		}
	}
	void Vulkan::CreateDescriptorSets()
	{
		std::vector<VkDescriptorSetLayout> layouts(SWAPCHAIN_IMAGE_COUNT, m_descriptorSetLayout);

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(m_swapchainImages.size());
		allocInfo.pSetLayouts = layouts.data();

		m_descriptorSets.resize(m_swapchainImages.size());
		VkResult result = vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.data());
		assert(result == VK_SUCCESS && "vkAllocateDescriptorSets() did not return success");

		// Allocate skeleton animation descriptor set
		std::vector<VkDescriptorSetLayout> animLayouts(SWAPCHAIN_IMAGE_COUNT, m_descriptorSetLayoutAnimation);

		VkDescriptorSetAllocateInfo allocInfoAnimation{};
		allocInfoAnimation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfoAnimation.descriptorPool = m_descriptorPool;
		allocInfoAnimation.descriptorSetCount = static_cast<uint32_t>(m_swapchainImages.size());
		allocInfoAnimation.pSetLayouts = animLayouts.data();

		m_descriptorSetsAnimation.resize(m_swapchainImages.size());
		result = vkAllocateDescriptorSets(m_device, &allocInfoAnimation, m_descriptorSetsAnimation.data());
		assert(result == VK_SUCCESS && "vkAllocateDescriptorSets() did not return success");

		// Allocate wind data descriptor set
		std::vector<VkDescriptorSetLayout> windLayouts(SWAPCHAIN_IMAGE_COUNT, m_descriptorSetLayoutWindData);

		VkDescriptorSetAllocateInfo allocInfoWindData{};
		allocInfoWindData.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfoWindData.descriptorPool = m_descriptorPool;
		allocInfoWindData.descriptorSetCount = static_cast<uint32_t>(m_swapchainImages.size());
		allocInfoWindData.pSetLayouts = windLayouts.data();

		m_descriptorSetsWindData.resize(m_swapchainImages.size());
		result = vkAllocateDescriptorSets(m_device, &allocInfoWindData, m_descriptorSetsWindData.data());
		assert(result == VK_SUCCESS && "vkAllocateDescriptorSets() did not return success");

		// Allocate grass descriptor set
		std::vector<VkDescriptorSetLayout> grassLayouts(SWAPCHAIN_IMAGE_COUNT, m_descriptorSetLayoutGrassCompute);

		VkDescriptorSetAllocateInfo allocInfoGrass{};
		allocInfoGrass.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfoGrass.descriptorPool = m_descriptorPool;
		allocInfoGrass.descriptorSetCount = static_cast<uint32_t>(m_swapchainImages.size());
		allocInfoGrass.pSetLayouts = grassLayouts.data();

		m_descriptorSetsGrassCompute.resize(m_swapchainImages.size());
		result = vkAllocateDescriptorSets(m_device, &allocInfoGrass, m_descriptorSetsGrassCompute.data());
		assert(result == VK_SUCCESS && "vkAllocateDescriptorSets() did not return success");

		// Bloom compute descriptor sets
		std::vector<VkDescriptorSetLayout> bloomLayouts(SWAPCHAIN_IMAGE_COUNT, m_descriptorSetLayoutBloomCompute);

		VkDescriptorSetAllocateInfo allocInfoBloom{};
		allocInfoBloom.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfoBloom.descriptorPool = m_descriptorPool;
		allocInfoBloom.descriptorSetCount = static_cast<uint32_t>(m_swapchainImages.size());
		allocInfoBloom.pSetLayouts = bloomLayouts.data();
		m_descriptorSetsBloomCompute.resize(m_swapchainImages.size());

		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfoBloom, m_descriptorSetsBloomCompute.data()));

		// Update
		UpdateDescriptorSets();
	}
	void Vulkan::UpdateDescriptorSets()
	{
		std::cout << "Updating descriptor sets..." << std::endl;

		std::cout << "Wind Data SSBO Size: " << m_windDataBufferSize << std::endl;

		for (size_t i = 0; i < m_swapchainImages.size(); i++)
		{
			std::array<VkWriteDescriptorSet, 11> descriptorWrites{};

			// UBOs
			
				VkDescriptorBufferInfo bufferInfoVert{};
				bufferInfoVert.buffer = m_uniformBuffersVert[i];
				bufferInfoVert.offset = 0;
				bufferInfoVert.range = sizeof(UniformBufferObjectVert);

				VkDescriptorBufferInfo bufferInfoFrag{};
				bufferInfoFrag.buffer = m_uniformBuffersFrag[i];
				bufferInfoFrag.offset = 0;
				bufferInfoFrag.range = sizeof(UniformBufferObjectFrag);

				descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[0].dstSet = m_descriptorSets[i];
				descriptorWrites[0].dstBinding = 0;
				descriptorWrites[0].dstArrayElement = 0;
				descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrites[0].descriptorCount = 1;
				descriptorWrites[0].pBufferInfo = &bufferInfoVert;

				descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[1].dstSet = m_descriptorSets[i];
				descriptorWrites[1].dstBinding = 1;
				descriptorWrites[1].dstArrayElement = 0;
				descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrites[1].descriptorCount = 1;
				descriptorWrites[1].pBufferInfo = &bufferInfoFrag;
			

			// Textures
			
				//std::vector<VkDescriptorImageInfo> imageInfo(m_textures.size());
				//for (int j = 0; j < m_textures.size(); ++j) {
				//	imageInfo[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				//	imageInfo[j].imageView = m_textures[j].view;
				//	imageInfo[j].sampler = m_sampler;
				//}
				//
				//descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				//descriptorWrites[2].dstSet = m_descriptorSets[i];
				//descriptorWrites[2].dstBinding = 2;
				//descriptorWrites[2].dstArrayElement = 0;
				//descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				//descriptorWrites[2].descriptorCount = m_textures.size();
				//descriptorWrites[2].pImageInfo = imageInfo.data();
			

			
				VkDescriptorImageInfo shadowMapInfo;
				shadowMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				shadowMapInfo.imageView = m_shadowMapObject.view;
				shadowMapInfo.sampler = m_sampler;

				descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[2].dstSet = m_descriptorSets[i];
				descriptorWrites[2].dstBinding = 3;
				descriptorWrites[2].dstArrayElement = 0;
				descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[2].descriptorCount = 1;
				descriptorWrites[2].pImageInfo = &shadowMapInfo;
			

			// Skeleton animation portion of descriptor sets
			
				VkDescriptorBufferInfo storageBufferInfoCurrentFrame{};
				storageBufferInfoCurrentFrame.buffer = m_ssboAnimation[i];
				storageBufferInfoCurrentFrame.offset = 0;
				storageBufferInfoCurrentFrame.range = sizeof(glm::mat4) * MAX_BONE_COUNT;

				descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[3].dstSet = m_descriptorSetsAnimation[i];
				descriptorWrites[3].dstBinding = 0;
				descriptorWrites[3].dstArrayElement = 0;
				descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				descriptorWrites[3].descriptorCount = 1;
				descriptorWrites[3].pBufferInfo = &storageBufferInfoCurrentFrame;
			

			// Grass compute
			
				VkDescriptorBufferInfo grassBufferInfo{};
				grassBufferInfo.buffer = m_ssboGrassCompute;
				grassBufferInfo.offset = 0;
				grassBufferInfo.range = GRASS_MAX_INSTANCE_COUNT * sizeof(Grass::Instance);

				descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[4].dstSet = m_descriptorSetsGrassCompute[i];
				descriptorWrites[4].dstBinding = 0;
				descriptorWrites[4].dstArrayElement = 0;
				descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				descriptorWrites[4].descriptorCount = 1;
				descriptorWrites[4].pBufferInfo = &grassBufferInfo;

				VkDescriptorBufferInfo grassLowBufferInfo{};
				grassLowBufferInfo.buffer = m_ssboGrassLowCompute;
				grassLowBufferInfo.offset = 0;
				grassLowBufferInfo.range = GRASS_MAX_INSTANCE_COUNT * sizeof(Grass::Instance);

				descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[5].dstSet = m_descriptorSetsGrassCompute[i];
				descriptorWrites[5].dstBinding = 1;
				descriptorWrites[5].dstArrayElement = 0;
				descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				descriptorWrites[5].descriptorCount = 1;
				descriptorWrites[5].pBufferInfo = &grassLowBufferInfo;

				VkDescriptorBufferInfo grassFlowerBufferInfo{};
				grassFlowerBufferInfo.buffer = m_ssboGrassFlowerCompute;
				grassFlowerBufferInfo.offset = 0;
				grassFlowerBufferInfo.range = GRASS_FLOWER_MAX_INSTANCE_COUNT * sizeof(Grass::Instance);

				descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[6].dstSet = m_descriptorSetsGrassCompute[i];
				descriptorWrites[6].dstBinding = 2;
				descriptorWrites[6].dstArrayElement = 0;
				descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				descriptorWrites[6].descriptorCount = 1;
				descriptorWrites[6].pBufferInfo = &grassFlowerBufferInfo;

				VkDescriptorBufferInfo grassBufferInfoUBO{};
				grassBufferInfoUBO.buffer = m_uboGrassCompute[i];
				grassBufferInfoUBO.offset = 0;
				grassBufferInfoUBO.range = sizeof(Grass::UBO);

				descriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[7].dstSet = m_descriptorSetsGrassCompute[i];
				descriptorWrites[7].dstBinding = 3;
				descriptorWrites[7].dstArrayElement = 0;
				descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrites[7].descriptorCount = 1;
				descriptorWrites[7].pBufferInfo = &grassBufferInfoUBO;

				VkDescriptorImageInfo grassBufferInfoTexture{};
				grassBufferInfoTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				grassBufferInfoTexture.imageView = m_grassComputeTexture.view;
				grassBufferInfoTexture.sampler = m_sampler;

				descriptorWrites[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[8].dstSet = m_descriptorSetsGrassCompute[i];
				descriptorWrites[8].dstBinding = 4;
				descriptorWrites[8].dstArrayElement = 0;
				descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[8].descriptorCount = 1;
				descriptorWrites[8].pImageInfo = &grassBufferInfoTexture;
			

			// Wind data ssbo
			
				VkDescriptorBufferInfo ssboWindDataInfo{};
				ssboWindDataInfo.buffer = m_ssboWindData[i];
				ssboWindDataInfo.offset = 0;
				ssboWindDataInfo.range = m_windDataBufferSize;

				descriptorWrites[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[9].dstSet = m_descriptorSetsWindData[i];
				descriptorWrites[9].dstBinding = 0;
				descriptorWrites[9].dstArrayElement = 0;
				descriptorWrites[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				descriptorWrites[9].descriptorCount = 1;
				descriptorWrites[9].pBufferInfo = &ssboWindDataInfo;

				VkDescriptorImageInfo windPerlinNoise{};
				windPerlinNoise.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				windPerlinNoise.imageView = m_perlinNoiseTexture.view;
				windPerlinNoise.sampler = m_sampler;

				descriptorWrites[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[10].dstSet = m_descriptorSetsWindData[i];
				descriptorWrites[10].dstBinding = 1;
				descriptorWrites[10].dstArrayElement = 0;
				descriptorWrites[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[10].descriptorCount = 1;
				descriptorWrites[10].pImageInfo = &windPerlinNoise;
			

			/*
* 			{
				// Bloom compute
				// Inputed color attachment
				VkDescriptorImageInfo bloomInputInfo;
				bloomInputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				bloomInputInfo.imageView = m_swapchainImageViews[i];
				bloomInputInfo.sampler = m_sampler;

				descriptorWrites[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[10].dstSet = m_descriptorSetsBloomCompute[i];
				descriptorWrites[10].dstBinding = 0;
				descriptorWrites[10].dstArrayElement = 0;
				descriptorWrites[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				descriptorWrites[10].descriptorCount = 1;
				descriptorWrites[10].pImageInfo = &bloomInputInfo;

				// Downsampled bloom images
				std::vector<VkDescriptorImageInfo> bloomImageInfo(BLOOM_SAMPLE_COUNT);
				for (int i = 0; i < BLOOM_SAMPLE_COUNT; ++i) // loop starts at 1!
				{
					bloomImageInfo[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
					bloomImageInfo[i].imageView = m_bloomImages[i].view;
					bloomImageInfo[i].sampler = m_sampler;
				}

				descriptorWrites[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[11].dstSet = m_descriptorSetsBloomCompute[i];
				descriptorWrites[11].dstBinding = 1;
				descriptorWrites[11].dstArrayElement = 0;
				descriptorWrites[11].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				descriptorWrites[11].descriptorCount = bloomImageInfo.size();
				descriptorWrites[11].pImageInfo = bloomImageInfo.data();
			}
			*/

			vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}

	void Vulkan::CreateIndexBuffer(std::vector<INDEX_TYPE>& in_indices, VkBuffer& in_buffer, VkDeviceMemory& in_memory)
	{
		std::cout << "Creating index buffer..." << std::endl;

		VkDeviceSize bufferSize = sizeof(in_indices[0]) * in_indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, in_indices.data(), (size_t)bufferSize);
		vkUnmapMemory(m_device, stagingBufferMemory);

		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, in_buffer, in_memory);

		CopyBuffer(stagingBuffer, in_buffer, bufferSize);

		vkDestroyBuffer(m_device, stagingBuffer, nullptr);
		vkFreeMemory(m_device, stagingBufferMemory, nullptr);
	}

	void Vulkan::UpdateLoadedIndexData(std::vector<INDEX_TYPE>& in_indices, VkBuffer& in_buffer, VkDeviceMemory& in_memory)
	{
		VkDeviceSize bufferSize = sizeof(in_indices[0]) * in_indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, in_indices.data(), (size_t)bufferSize);
		vkUnmapMemory(m_device, stagingBufferMemory);

		vkDestroyBuffer(m_device, in_buffer, nullptr);
		vkFreeMemory(m_device, in_memory, nullptr);
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, in_buffer, in_memory);

		CopyBuffer(stagingBuffer, in_buffer, bufferSize);

		vkDestroyBuffer(m_device, stagingBuffer, nullptr);
		vkFreeMemory(m_device, stagingBufferMemory, nullptr);
	}
	void Vulkan::UpdateLoadedTextureData()
	{
		UpdateDescriptorSets();
	}

	// TODO: query and be checking physical device props all throughout code
	void Vulkan::CreateUniformBuffers()
	{
		std::cout << "Creating Uniform Buffers..." << std::endl;

		VkDeviceSize bufferSizeVert = sizeof(UniformBufferObjectVert);
		VkDeviceSize bufferSizeFrag = sizeof(UniformBufferObjectFrag);
		VkDeviceSize bufferSizeGrassCompute = sizeof(Grass::UBO);

		m_uniformBuffersVert.resize(m_swapchainImages.size());
		m_uniformBuffersMemoryVert.resize(m_swapchainImages.size());

		m_uniformBuffersFrag.resize(m_swapchainImages.size());
		m_uniformBuffersMemoryFrag.resize(m_swapchainImages.size());

		m_uboGrassCompute.resize(m_swapchainImages.size());
		m_uboGrassComputeMemory.resize(m_swapchainImages.size());

		for (size_t i = 0; i < m_swapchainImages.size(); i++) {
			CreateBuffer(bufferSizeVert, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniformBuffersVert[i], m_uniformBuffersMemoryVert[i]);

			CreateBuffer(bufferSizeFrag, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniformBuffersFrag[i], m_uniformBuffersMemoryFrag[i]);

			CreateBuffer(bufferSizeGrassCompute, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uboGrassCompute[i], m_uboGrassComputeMemory[i]);
		}
	}
	void Vulkan::UpdateUniformBuffer(uint32_t in_imageIndex, const Scene* in_pScene)
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		float zoom = 12.0f;
		// Vertex UBO
		// IF THIS IS BREAKING CAMERA IS PROBABLY NULL
		Vec2 screenHalfDim = Vec2(m_swapchainExtent.width / 2.0f, m_swapchainExtent.height / 2.0f);
		UniformBufferObjectVert uboVert{};
		//Mega::ViewData vd = m_pCamera->GetViewData();
		uboVert.view = m_pCamera->GetLookAtMatrix();
		uboVert.proj = m_pCamera->GetProjectionMatrix(m_swapchainExtent.width / (float)m_swapchainExtent.height, 0.1f, 5000.0f); // glm::ortho(-screenHalfDim.x / zoom, screenHalfDim.x / zoom, -screenHalfDim.y / zoom, screenHalfDim.y / zoom, -1000.0f, 1000.0f); // glm::ortho(-screenHalfDim.x, screenHalfDim.x, -screenHalfDim.y, screenHalfDim.y);;
		uboVert.proj[1][1] *= -1; // Flipping the Y coordinates because opengl uses inverted y coordinates

		void* dataVert;
		vkMapMemory(m_device, m_uniformBuffersMemoryVert[in_imageIndex], 0, sizeof(uboVert), 0, &dataVert);
		memcpy(dataVert, &uboVert, sizeof(uboVert));
		vkUnmapMemory(m_device, m_uniformBuffersMemoryVert[in_imageIndex]);

		// Fragment UBO

		UniformBufferObjectFrag uboFrag{};
		uboFrag.lightSpaceMat = g_lightSpaceMat;
		uboFrag.viewDir = m_pCamera->GetDirection();
		uboFrag.viewPos = m_pCamera->GetPosition();

		// Lights
		int count = 0;
		auto view = in_pScene->GetRegistry().view<const Component::Light>();
		for (const auto& [entity, l] : view.each())
		{
			uboFrag.lights[count] = l.lightData;
			count++;
		}
		uboFrag.lightCount = count;

		void* dataFrag;
		vkMapMemory(m_device, m_uniformBuffersMemoryFrag[in_imageIndex], 0, VK_WHOLE_SIZE, 0, &dataFrag);
		memcpy(dataFrag, &uboFrag, sizeof(uboFrag));
		vkUnmapMemory(m_device, m_uniformBuffersMemoryFrag[in_imageIndex]);

		// Grass Compute UBO
		Grass::UBO uboGrass{};

		// From lighthouse3d.com and learnopengl frustum culling
		glm::vec3 cameraPos = m_pCamera->GetPosition();
		glm::vec3 cameraDir = normalize(m_pCamera->GetDirection()); // TODO: camera should normalize in getter?
		glm::vec3 cameraUp = normalize(m_pCamera->GetUp());
		glm::vec3 cameraRight = -normalize(glm::cross(cameraUp, cameraDir)); // TODO: add getRight to camera class

		//ImGui::DragFloat3("Camera Up", &cameraUp.x);
		//ImGui::DragFloat3("Camera Right", &cameraRight.x);
		//ImGui::DragFloat3("Camera Forward", &cameraDir.x);

		const float aspect = m_swapchainExtent.width / (float)m_swapchainExtent.height;
		const float fovY = glm::radians(45.0f); // TODO: get from camera
		const float fovX = fovY * aspect;

		const float farDist = 5000.0f; // TODO: global near/far/fov values for the camera (probably just add them to the camera class)
		const float nearDist = 0.1f;

		// Frustum culling for grass compute shader
		const float halfVSide = farDist * tan(fovY / 2.0f);
		const float halfHSide = halfVSide * aspect;
		const glm::vec3 frontMultFar = farDist * cameraDir;

		Grass::Frustum frustum;
		frustum.front  = { cameraPos + nearDist * cameraDir, cameraDir };
		frustum.back   = { cameraPos + frontMultFar, -cameraDir };
		frustum.right  = { cameraPos, glm::cross(frontMultFar - cameraRight * halfHSide, cameraUp) };
		frustum.left   = { cameraPos, glm::cross(cameraUp, frontMultFar + cameraRight * halfHSide) };
		frustum.top    = { cameraPos, glm::cross(cameraRight, frontMultFar - cameraUp * halfVSide) };
		frustum.bottom = { cameraPos, glm::cross(frontMultFar + cameraUp * halfVSide, cameraRight) };

		uboGrass.frustum = frustum;

		uboGrass.r = g_r;
		uboGrass.time = (float)(Mega::Time() - m_startTime);

		uboGrass.viewPos = glm::vec4(cameraPos, 1);
		uboGrass.viewDir = glm::vec4(cameraDir, 1);
		uboGrass.windSimCenter = Vec4(Engine::Get()->m_pWindSystem->GetWindSimulationCenter(), 0);

		void* dataGrass;
		vkMapMemory(m_device, m_uboGrassComputeMemory[(in_imageIndex)], 0, VK_WHOLE_SIZE, 0, &dataGrass); // TODO checks to make sure mapping works
		memcpy(dataGrass, &uboGrass, sizeof(Grass::UBO));
		vkUnmapMemory(m_device, m_uboGrassComputeMemory[(in_imageIndex)]);

		// Animation SSBO
		auto& modelMats = Mega::Engine::Get()->m_pAnimationSystem->GetModelMatData();
		memcpy(m_animStagingBufferMap, modelMats.data(), (size_t)(std::size(modelMats) * sizeof(modelMats[0]))); //(size_t)m_animBufferSize); // TODO: memmcpy bug
		CopyBuffer(m_animStagingBuffer, m_ssboAnimation[in_imageIndex], m_animBufferSize); // TODO: dont need 3 copies of GPU buffers?

		// Wind Data SSBO
		{
			// Global wind vector
			glm::vec4 globalWindVec = Mega::Engine::Get()->m_pWindSystem->GetGlobalWindVector();
			auto& globalWindData = Mega::Engine::Get()->m_pWindSystem->GetGlobalWindData();

			// Wind simulation
			const size_t size = std::size(globalWindData) * sizeof(globalWindData[0]);
			MEGA_ASSERT(size == m_windDataBufferSize - sizeof(glm::vec4), "Different sizes between scene wind data and vulkan's wind data buffer");

			std::memcpy((void*)(((glm::vec4*)m_windDataStagingBufferMap) + 1), globalWindData.data(), size);
			std::memcpy(m_windDataStagingBufferMap, &globalWindVec, sizeof(glm::vec4));
			CopyBuffer(m_windDataStagingBuffer, m_ssboWindData[in_imageIndex], m_windDataBufferSize);
		}
	}

	Vulkan::Pipeline Vulkan::CreateGraphicsPipeline(const char* in_vertShader, const char* in_fragShader, const size_t in_pushSize, const char* in_name, bool in_animated, bool in_grass)
	{
		std::cout << "Creating graphics pipeline..." << std::endl;
		Pipeline out_pipeline;
		out_pipeline.name = in_name;
		out_pipeline.pVulkanInstance = this;

		assert(m_device != nullptr && "Cannot create a graphics pipeline with a null logical device");

		out_pipeline.vertexShader = ShaderHelper::CompileAndCreateShader(m_device, in_vertShader + std::string(".vert"), in_vertShader + std::string(".spv"));
		out_pipeline.fragmentShader = ShaderHelper::CompileAndCreateShader(m_device, in_fragShader + std::string(".frag"), in_fragShader + std::string(".spv"));

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = out_pipeline.vertexShader;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = out_pipeline.fragmentShader;
		fragShaderStageInfo.pName = "main";

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		// Now we want ot be able to accept data from vertex buffers and pass it to our shaders
		VkVertexInputBindingDescription bindingDescription;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
		if (in_animated)
		{
			bindingDescription = AnimatedVertex::GetBindingDescription();
			attributeDescriptions = AnimatedVertex::GetAttributeDescriptions();
		}
		//else if (in_grass)
		//{
		//	bindingDescription = GrassVertex::GetBindingDescription();
		//	attributeDescriptions = GrassVertex::GetAttributeDescriptions();
		//}
		else
		{
			bindingDescription = Vertex::GetBindingDescription();
			attributeDescriptions = Vertex::GetAttributeDescriptions();
		}

		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{}; // Basically how Vulkan will draw the vertices we give it
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f; // Optional
		depthStencil.maxDepthBounds = 1.0f; // Optional
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {}; // Optional
		depthStencil.back = {}; // Optional

		VkViewport viewport{}; // What portion of the framebuffer we want to render to
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)m_swapchainExtent.width;
		viewport.height = (float)m_swapchainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{}; // What part of the image we want to render
		scissor.offset = { 0, 0 };
		scissor.extent = m_swapchainExtent;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer{}; // Performs depth testing, face cullingand the scissor test
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = CULL_MODE;
		if (in_grass)
		{
			rasterizer.cullMode = VK_CULL_MODE_NONE;
		}

		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_TRUE;
		rasterizer.depthBiasConstantFactor = 0.0f; // Optional
		rasterizer.depthBiasClamp = 0.0f; // Optional
		rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		if (ENABLE_SAMPLE_SHADING)
		{
			multisampling.sampleShadingEnable = VK_TRUE; // enable sample shading in the pipeline
			multisampling.minSampleShading = 0.2f; // min fraction for sample shading; closer to one is smooth
			std::cout << "SAMPLE SHADING ENABLED - SAMPLE COUNT: " << m_msaaSamples << std::endl;
		}
		multisampling.rasterizationSamples = m_msaaSamples;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH,
		};

		VkPipelineDynamicStateCreateInfo dynamicState{}; // Stuff that can be changed without recreating the pipeline
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
		dynamicState.pDynamicStates = dynamicStates.data();

		std::vector<VkDescriptorSetLayout> layouts;
		layouts.push_back(m_descriptorSetLayout);
		if (in_animated)
		{
			layouts.push_back(m_descriptorSetLayoutAnimation);
		}
		else if (in_grass)
		{
			layouts.push_back(m_descriptorSetLayoutGrassCompute);
		}

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size()); // Optional
		pipelineLayoutInfo.pSetLayouts = layouts.data(); // Optional

		std::vector<VkPushConstantRange> pushRanges;
		if (in_pushSize > 0)
		{
			std::cout << "Push Constant Size: " << in_pushSize << std::endl;

			VkPushConstantRange pushConstantRangeVert{};
			pushConstantRangeVert.offset = 0;
			pushConstantRangeVert.size = (uint32_t)in_pushSize;
			pushConstantRangeVert.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

			pushRanges.push_back(pushConstantRangeVert);
		}

		pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushRanges.size());
		pipelineLayoutInfo.pPushConstantRanges = pushRanges.data();

		VkResult result = vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &out_pipeline.layout);
		assert(result == VK_SUCCESS && "ERROR: vkCreatePipelineLayout() did not return success");

		VkPipelineShaderStageCreateInfo shaderStages[2] = { vertShaderStageInfo, fragShaderStageInfo }; // Might cause read access violation error

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.layout = out_pipeline.layout;
		pipelineInfo.renderPass = m_renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.pDepthStencilState = &depthStencil;

		result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &out_pipeline.graphicsPipeline);
		assert(result == VK_SUCCESS && "ERROR: vkCreateGraphicsPipelines() did not return sucess");

		return out_pipeline;
	}

	Vulkan::Pipeline Vulkan::CreateShadowMapPipeline(const char* in_vertShader, const uint32_t in_pushSize, const bool in_animated)
	{
		Pipeline out_pipeline;
		out_pipeline.pVulkanInstance = this;

		// Only want the position vertex data (no color, normal, etc) so we create new bindings here
		VkVertexInputBindingDescription vertexBinding{};
		std::vector<VkVertexInputAttributeDescription> vertexAttributes{};
		//

		if (in_animated)
		{
			vertexBinding = AnimatedVertex::GetBindingDescription();
			vertexAttributes = AnimatedVertex::GetAttributeDescriptions();
		}
		else
		{
			vertexBinding = Vertex::GetBindingDescription();

			VkVertexInputAttributeDescription va;
			va.binding = 0;
			va.location = 0;
			va.format = VK_FORMAT_R32G32B32_SFLOAT;
			va.offset = 0;
			vertexAttributes.push_back(va);
		}

		VkPipelineVertexInputStateCreateInfo vertexInfo{};
		vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInfo.pNext = NULL;
		vertexInfo.flags = 0;
		vertexInfo.vertexBindingDescriptionCount = 1;
		vertexInfo.pVertexBindingDescriptions = &vertexBinding;
		vertexInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size());
		vertexInfo.pVertexAttributeDescriptions = vertexAttributes.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{}; // Basically how Vulkan will draw the vertices we give it
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f; // Optional
		depthStencil.maxDepthBounds = 0.0f; // Optional
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {}; // Optional
		depthStencil.back = {}; // Optional

		VkViewport viewport{}; // What portion of the framebuffer we want to render to
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = SHADOW_MAP_WIDTH;
		viewport.height = SHADOW_MAP_HEIGHT;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{}; // What part of the image we want to render
		scissor.offset = { 0, 0 };
		scissor.extent.width = SHADOW_MAP_WIDTH;
		scissor.extent.height = SHADOW_MAP_HEIGHT;

		VkPipelineRasterizationStateCreateInfo rasterizer{}; // Performs depth testing, face cullingand the scissor test
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT; // Shadow map culls front faces
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_TRUE;
		rasterizer.depthBiasConstantFactor = SHADOW_MAP_DEPTH_BIAS_CONST; // 0.0f; // Optional
		rasterizer.depthBiasClamp = 0.0f; // Optional
		rasterizer.depthBiasSlopeFactor = SHADOW_MAP_DEPTH_BIAS_SLOPE; // Optional

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		out_pipeline.vertexShader = ShaderHelper::CompileAndCreateShader(m_device, in_vertShader + std::string(".vert"), in_vertShader + std::string(".spv"));
		VkPipelineShaderStageCreateInfo shaderStageVert{};
		shaderStageVert.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageVert.flags = 0;
		shaderStageVert.stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStageVert.pName = "main";
		shaderStageVert.module = out_pipeline.vertexShader;

		std::vector<VkPushConstantRange> pushConstantRanges{};
		if (in_pushSize > 0)
		{
			VkPushConstantRange pc;
			pc.offset = 0;
			pc.size = in_pushSize;
			pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			pushConstantRanges.push_back(pc);
		}

		std::vector<VkDescriptorSetLayout> layouts;
		layouts.push_back(m_descriptorSetLayout);
		if (in_animated)
		{
			layouts.push_back(m_descriptorSetLayoutAnimation);
		}

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
		pipelineLayoutInfo.pSetLayouts = layouts.data();
		pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
		pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());

		VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &out_pipeline.layout));

		VkGraphicsPipelineCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

		std::array<VkPipelineShaderStageCreateInfo, 1> shaderStages = { shaderStageVert };
		createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		createInfo.pStages = shaderStages.data();

		createInfo.pVertexInputState = &vertexInfo;
		createInfo.pInputAssemblyState = &inputAssembly; // TODO: figure out what states you dont need here
		createInfo.pViewportState = &viewportState;
		createInfo.pRasterizationState = &rasterizer;
		createInfo.pMultisampleState = &multisampling;
		//createInfo.pColorBlendState = &colorBlending;
		createInfo.layout = out_pipeline.layout;
		createInfo.renderPass = m_renderPassShadowMap;
		createInfo.subpass = 0;
		createInfo.pDepthStencilState = &depthStencil;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &out_pipeline.graphicsPipeline));

		return out_pipeline;
	}

	void Vulkan::CreateRenderPass()
	{
		// Set up to expect a framebuffer, which is used as render target

		std::cout << "Creating render pass..." << std::endl;

		assert(m_device != nullptr && "Cannot create a render pass with a null logical device");

		// "The attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer object.
		// A framebuffer object references all of the VkImageView objects that represent the attachments.
		// In our case that will be only a single one : the color attachment."
		// In other words, a framebuffer object refernces the corresponding VkImageView object that represents the attachments

		// COLOR - PRE PROCESSING
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = m_surfaceFormat.format;
		colorAttachment.samples = m_msaaSamples;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// DEPTH
		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = FindDepthFormat();
		depthAttachment.samples = m_msaaSamples;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// MULTI SAMPLING
		// VkAttachmentDescription colorAttachmentResolve{};
		// colorAttachmentResolve.format = m_surfaceFormat.format;
		// colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		// colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		// colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		// colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		// colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		// colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		// colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		// 
		// VkAttachmentReference colorAttachmentResolveRef{};
		// colorAttachmentResolveRef.attachment = 2;
		// colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// SUBPASSES
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
		//subpass.pResolveAttachments = &colorAttachmentResolveRef;

		// DEPENDENCIES
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // If not presenting anymore, need to change
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		// RENDER PASS
		std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment }; // , colorAttachmentResolve };
		std::vector<VkSubpassDescription> subpasses = { subpass };

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = (uint32_t)subpasses.size();
		renderPassInfo.pSubpasses = subpasses.data();
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VkResult result = vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass);
		assert(result == VK_SUCCESS && "ERROR: vkCreateRenderPass() did not return success");


		{
			// RENDER PASS FOR SHADOW MAP
			// Shadow map depth attachment
			VkAttachmentDescription depth{};
			depth.samples = VK_SAMPLE_COUNT_1_BIT;
			depth.format = SHADOW_MAP_FORMAT; // Make sure this format is supported
			depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depth.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			// Attachment references from subpasses
			VkAttachmentReference depthRef{};
			depthRef.attachment = 0;
			depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			// Subpass 0: shadow map rendering
			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.pColorAttachments = VK_NULL_HANDLE; // No color attachments for shadow mapping
			subpass.pResolveAttachments = VK_NULL_HANDLE;
			subpass.pDepthStencilAttachment = &depthRef;

			// Create render pass
			VkRenderPassCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			createInfo.attachmentCount = 1;
			createInfo.pAttachments = &depth;
			createInfo.subpassCount = 1;
			createInfo.pSubpasses = &subpass;

			VK_CHECK_RESULT(vkCreateRenderPass(m_device, &createInfo, NULL, &m_renderPassShadowMap));
		}
	}

	void Vulkan::CreateFramebuffers(std::vector<VkFramebuffer>& in_swapchainFramebuffers)
	{
		// Framebuffers are used as the render target in the render pass

		std::cout << "Creating framebuffers..." << std::endl;

		assert(m_device != nullptr && "Cannot create framebuffers with a null logical device");

		in_swapchainFramebuffers.resize(m_swapchainImageViews.size());

		for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
			std::array<VkImageView, 2> attachments = {
				// m_colorObject.view, // MULTI SAMPLING
				m_swapchainImageViews[i],
				m_depthObject.view,
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_renderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = m_swapchainExtent.width;
			framebufferInfo.height = m_swapchainExtent.height;
			framebufferInfo.layers = 1;

			VkResult result = vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &in_swapchainFramebuffers[i]);
			assert(result == VK_SUCCESS && "ERROR: vkCreateFramebuffer() did not return success");
		}

		// Shadow map framebuffers
		m_shadowMapFramebuffers.resize(m_swapchainImageViews.size());

		for (size_t i = 0; i < m_swapchainImageViews.size(); i++)
		{
			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_renderPassShadowMap;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = &m_shadowMapObject.view;
			framebufferInfo.width = SHADOW_MAP_WIDTH;
			framebufferInfo.height = SHADOW_MAP_HEIGHT;
			framebufferInfo.layers = 1;

			VkResult result = vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_shadowMapFramebuffers[i]);
			assert(result == VK_SUCCESS && "ERROR: vkCreateFramebuffer() did not return success");
		}
	}

	void Vulkan::CreateBloom()
	{
		// Create pipeline
		CreateComputePipeline("Shaders/bloomCompute", &m_bloomComputePipeline, 0, true);

		// inputted image
		{
			Vec2U imageExtent = { m_swapchainExtent.width, m_swapchainExtent.height };

			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width = imageExtent.x;
			imageInfo.extent.height = imageExtent.y;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

			CreateImageObject(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_bloomInput.image, m_bloomInput.memory);
			CreateImageView(m_bloomInput.view, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_ASPECT_COLOR_BIT, m_bloomInput.image);
			ChangeImageLayout(m_bloomInput.image, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		}

		// downsampled images
		{
			Vec2 imageExtent = { m_swapchainExtent.width, m_swapchainExtent.height };

			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width  = (uint32_t)imageExtent.x;
			imageInfo.extent.height = (uint32_t)imageExtent.y;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

			m_bloomImages.resize(BLOOM_SAMPLE_COUNT);
			for (ImageObject& image : m_bloomImages)
			{
				// Downsize image extent
				//imageExtent *= 0.5f;
				MEGA_ASSERT(imageExtent.x > 0 && imageExtent.y > 0, "Bloom downscaling to small!");

				imageInfo.extent.width  = (uint32_t)imageExtent.x;
				imageInfo.extent.height = (uint32_t)imageExtent.y;

				CreateImageObject(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image.image, image.memory);
				CreateImageView(image.view, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_ASPECT_COLOR_BIT, image.image);
				ChangeImageLayout(image.image, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			}
		}
	}

	void Vulkan::DestroyBloom()
	{
		// TODO
		//vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutBloom, nullptr);
	
		// Destroy pipeline
		ComputePipeline::Destroy(this, m_bloomComputePipeline);
	
		// Destroy images
		ImageObject::Destroy(&m_device, &m_bloomInput);
		for (int i = 0; i < m_bloomImages.size(); i++)
		{
			ImageObject::Destroy(&m_device, &m_bloomImages[i]);
		}
	}

	void Vulkan::CreateDrawCommands(std::vector<VkCommandBuffer>& in_buffers, std::vector<VkCommandPool>& in_pools)
	{
		// Creates a command buffer for each image in the swapchain

		std::cout << "Creating draw commands..." << std::endl;

		assert(m_device != nullptr && "ERROR: Cannot create a commands without a logical device");
		assert(m_renderPass != nullptr && "ERROR: Cannot create a commands without a render pass");

		in_buffers.resize(m_swapchainFramebuffers.size());
		m_grassComputeCommandBuffers.resize(m_swapchainFramebuffers.size());
		m_shadowMapCommandBuffers.resize(m_swapchainFramebuffers.size());
		m_bloomComputeCommandBuffers.resize(m_swapchainFramebuffers.size());

		for (int i = 0; i < m_swapchainFramebuffers.size(); ++i) {
			assert(in_pools[i] != nullptr && "ERROR: Cannot create a command without a command pool");

			// Normal on screen drawing
			AllocateCommandBuffer(in_buffers[i], in_pools[i]);

			// Grass compute
			AllocateCommandBuffer(m_grassComputeCommandBuffers[i], in_pools[i]);

			// Shadow map
			AllocateCommandBuffer(m_shadowMapCommandBuffers[i], in_pools[i]);

			// Bloom compute
			AllocateCommandBuffer(m_bloomComputeCommandBuffers[i], in_pools[i]);
		}

	}
	void Vulkan::CreateDrawCommandPools(std::vector<VkCommandPool>& in_pools)
	{
		// Creates a command pool for each frame in the swapchain

		std::cout << "Creating draw command pools..." << std::endl;

		assert(m_device != nullptr && "ERROR: Cannot create a command pool using a null logical device");

		QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_physicalDevice, m_surface);

		in_pools.resize(m_swapchainImageViews.size());

		for (int i = 0; i < in_pools.size(); ++i) {
			VkCommandPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsAndComputeFamily.value();
			poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Optional

			VkResult result = vkCreateCommandPool(m_device, &poolInfo, nullptr, &in_pools[i]);
			assert(result == VK_SUCCESS && "ERROR: vkCreateCommanPool() did not return success");
		}
	}
	void Vulkan::AllocateCommandBuffer(VkCommandBuffer& in_buffer, const VkCommandPool& in_pool)
	{
		// Allocates a command buffer into the command pool

		assert(m_device != nullptr && "ERROR: Cannot create a command buffer without a logical device");
		assert(in_pool != nullptr && "ERROR: Cannot allocate a command buffer without a command pool");

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = in_pool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = uint32_t(1);

		VkResult result = vkAllocateCommandBuffers(m_device, &allocInfo, &in_buffer);
		assert(result == VK_SUCCESS && "ERROR: vkAllocateout_commandBuffers() did not return success");
	}

	VkCommandBuffer Vulkan::BeginSingleTimeCommand(const VkCommandPool& in_pool)
	{
		// Creates, begins and returns a single time command buffer in the given command pool
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = in_pool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer out_command;
		vkAllocateCommandBuffers(m_device, &allocInfo, &out_command);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(out_command, &beginInfo);

		return out_command;
	}
	void Vulkan::EndSingleTimeCommand(const VkCommandPool& in_pool, VkCommandBuffer in_command)
	{
		// Ends, submits into the graphics queue, and frees the given command buffer

		vkEndCommandBuffer(in_command);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &in_command;

		VkResult result = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(result == VK_SUCCESS && "ERROR: vkQueueSubmit() did not return success");

		vkQueueWaitIdle(m_graphicsQueue);

		vkFreeCommandBuffers(m_device, in_pool, 1, &in_command);
	}

	void Vulkan::CreateDepthResources(ImageObject& in_depthObject)
	{
		VkFormat depthFormat = FindDepthFormat();

		CreateImage(m_swapchainExtent.width, m_swapchainExtent.height, 1, m_msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL, \
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, in_depthObject.image, in_depthObject.memory);
		CreateImageView(in_depthObject.view, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, in_depthObject.image);

		// ================= Shadow map depth object ======================= // TODO: use createimage and createimageview helper functions
		VkImageCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.pNext = NULL;
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = SHADOW_MAP_FORMAT;
		info.extent.width = SHADOW_MAP_WIDTH;
		info.extent.height = SHADOW_MAP_HEIGHT;
		info.extent.depth = 1;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices = NULL;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.flags = 0;

		vkCreateImage(m_device, &info, NULL, &m_shadowMapObject.image);

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_device, m_shadowMapObject.image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_shadowMapObject.memory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(m_device, m_shadowMapObject.image, m_shadowMapObject.memory, 0);

		VkImageViewCreateInfo view_info = {};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.pNext = NULL;
		view_info.image = m_shadowMapObject.image;
		view_info.format = SHADOW_MAP_FORMAT;
		view_info.components.r = VK_COMPONENT_SWIZZLE_R;
		view_info.components.g = VK_COMPONENT_SWIZZLE_G;
		view_info.components.b = VK_COMPONENT_SWIZZLE_B;
		view_info.components.a = VK_COMPONENT_SWIZZLE_A;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.flags = 0;

		vkCreateImageView(m_device, &view_info, NULL, &m_shadowMapObject.view);
	}
	// MULTI SAMPLING
	void Vulkan::CreateColorResources(ImageObject& in_colorObject)
	{
		VkFormat colorFormat = m_surfaceFormat.format;

		// TODO: if dont need it, this should not have VK_IMAGE_USAGE_TRANSFER_DST_BIT or VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		CreateImage(m_swapchainExtent.width, m_swapchainExtent.height, 1, m_msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL, \
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, in_colorObject.image, in_colorObject.memory);
		CreateImageView(in_colorObject.view, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, in_colorObject.image);
	}
	VkFormat Vulkan::FindSupportedFormat(const std::vector<VkFormat>& in_candidates, VkImageTiling in_tiling, VkFormatFeatureFlags in_features)
	{
		// Some helpers to find if the supported image format is available for the depth image
		for (VkFormat format : in_candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

			if (in_tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & in_features) == in_features) {
				return format;
			}
			else if (in_tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & in_features) == in_features) {
				return format;
			}
		}

		std::runtime_error("ERROR: Could not find a supported format");

		return VkFormat(-1);
	}
	VkFormat Vulkan::FindDepthFormat()
	{
		return FindSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}
	bool Vulkan::HasStencilComponent(VkFormat format)
	{
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	void Vulkan::CreateSyncObjects() // Create the semaphores and fences
	{
		m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT); // TODO: use just max_in_flight or m_swapchain.size()
		m_shadowMapFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_bloomFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

		m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		m_imagesInFlight.resize(m_swapchainImages.size(), VK_NULL_HANDLE);

		m_computeInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		m_computeFinishedSemaphores.resize(m_swapchainImages.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			VkResult result1 = vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]);
			VkResult result2 = vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]);
			VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_shadowMapFinishedSemaphores[i]));
			VkResult result3 = vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]);

			VkResult result4 = vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_computeFinishedSemaphores[i]);
			VkResult result5 = vkCreateFence(m_device, &fenceInfo, nullptr, &m_computeInFlightFences[i]);

			VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_bloomFinishedSemaphores[i]))

				assert(result1 == VK_SUCCESS && result2 == VK_SUCCESS && result3 == VK_SUCCESS && result4 == VK_SUCCESS && result5 == VK_SUCCESS &&
					"ERROR: vkCreateSemaphore() or vkCreateFence() did not return success");
		}
	}

	// ================================ Other Functions ============================= //

	void Vulkan::CreateTextureSampler(VkSampler in_sampler)
	{
		// "Textures are usually accessed through samplers, which will apply filtering and transformations to compute the final color that is retrieved."
		// for example, bilinear filtering, or anisotropic filtering

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		//samplerInfo.magFilter = VK_FILTER_LINEAR;
		//samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // Repeat the texture when going beyond the image dimensions.
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		// samplerInfo.anisotropyEnable = VK_TRUE;
		// samplerInfo.maxAnisotropy = m_physicalDeviceProperties.limits.maxSamplerAnisotropy;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = m_physicalDeviceProperties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		VkResult result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler);
		assert(result == VK_SUCCESS && "ERROR: vkCreateSampler() did not return success");
	}
	void Vulkan::CreateTextureSamplerPost()
	{
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // Repeat the texture when going beyond the image dimensions.
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		// samplerInfo.anisotropyEnable = VK_TRUE;
		// samplerInfo.maxAnisotropy = m_physicalDeviceProperties.limits.maxSamplerAnisotropy;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = m_physicalDeviceProperties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		VkResult result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_samplerPost);
		assert(result == VK_SUCCESS && "ERROR: vkCreateSampler() did not return success");
	}

	void Vulkan::CreateImageObject(VkImageCreateInfo& in_info, VkMemoryPropertyFlags in_properties, VkImage& in_image, VkDeviceMemory& in_imageMemory)
	{
		// Creates and vkImage using the given info and passed image, allocates memory for it, and binds the given image and image memory
		// Still need to create an image view, should prob make that one function tho

		VkResult result = vkCreateImage(m_device, &in_info, nullptr, &in_image);
		assert(result == VK_SUCCESS && "vkCreateImage() did not return success");

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_device, in_image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, in_properties);

		result = vkAllocateMemory(m_device, &allocInfo, nullptr, &in_imageMemory);
		assert(result == VK_SUCCESS && "vkAllocateMemory() did not return success");

		vkBindImageMemory(m_device, in_image, in_imageMemory, 0);
	}
	void Vulkan::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, \
		VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = numSamples;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(m_device, image, imageMemory, 0);
	}
	void Vulkan::ChangeImageLayout(VkImage& in_image, VkFormat in_format, VkImageLayout in_old, VkImageLayout in_new)
	{
		// In order to use vkCmdCopyBufferToImage(), the image has to be in the correct layout

		VkCommandBuffer commandBuffer = BeginSingleTimeCommand(m_drawCommandPools[0]);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = in_old;
		barrier.newLayout = in_new;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = in_image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage{};
		VkPipelineStageFlags destinationStage{};

		if (in_old == VK_IMAGE_LAYOUT_UNDEFINED && in_new == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (in_old == VK_IMAGE_LAYOUT_UNDEFINED && in_new == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else if (in_old == VK_IMAGE_LAYOUT_UNDEFINED && in_new == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (in_old == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && in_new == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (in_old == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && in_new == VK_IMAGE_LAYOUT_GENERAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (in_old == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && in_new == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (in_old == VK_IMAGE_LAYOUT_UNDEFINED && in_new == VK_IMAGE_LAYOUT_GENERAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (in_old == VK_IMAGE_LAYOUT_GENERAL && in_new == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = 0;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		}
		else if (in_old == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && in_new == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else
		{
			assert(false);
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		EndSingleTimeCommand(m_drawCommandPools[0], commandBuffer);
	}
	void Vulkan::CopyBufferToImage(VkBuffer& in_buffer, VkImage& in_image, uint32_t in_width, uint32_t in_height)
	{
		VkCommandBuffer command = BeginSingleTimeCommand(m_drawCommandPools[0]);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { in_width, in_height, 1 };

		vkCmdCopyBufferToImage(
			command,
			in_buffer,
			in_image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // Assuming the image layout has already been transitioned to this
			1,
			&region
		);

		EndSingleTimeCommand(m_drawCommandPools[0], command);
	}
	void Vulkan::CreateImageView(VkImageView& in_view, VkFormat in_format, VkImageAspectFlags in_aspectFlags, VkImage& in_image)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = in_image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = in_format;
		viewInfo.subresourceRange.aspectMask = in_aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkResult result = vkCreateImageView(m_device, &viewInfo, nullptr, &in_view);
		assert(result == VK_SUCCESS && "ERROR: vkCreateImageView() did not return success");
	}

	void Vulkan::CreateSceneImages()
	{
		uint32_t width = m_swapchainExtent.width;
		uint32_t height = m_swapchainExtent.height;
		VkFormat format = m_surfaceFormat.format;

		m_sceneImages.resize(m_swapchainImages.size());
		for (int i = 0; i < m_swapchainImages.size(); ++i)
		{
			CreateImage(width, height, 1, m_msaaSamples, format, VK_IMAGE_TILING_OPTIMAL, \
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_sceneImages[i].image, m_sceneImages[i].memory);
			CreateImageView(m_sceneImages[i].view, format, VK_IMAGE_ASPECT_COLOR_BIT, m_sceneImages[i].image); // VK_IMAGE_USAGE_SAMPLED_BIT is a different enum
		}
	}

	void Vulkan::DestroySceneImages()
	{
		for (int i = 0; i < m_sceneImages.size(); ++i)
		{
			ImageObject::Destroy(&m_device, &m_sceneImages[i]);
		}
	}

	// ================================ Helper Functions ============================= //

	uint32_t Vulkan::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) // "Graphics cards can offer different types of memory to allocate from.Each type of
	{ 																					   // memory varies in terms of allowed operationsand performance characteristics. We need to combine the
		VkPhysicalDeviceMemoryProperties memoryProps;									   // requirements of the bufferand our own application requirements to find the right type of memory to use."
		vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProps);

		for (uint32_t i = 0; i < memoryProps.memoryTypeCount; i++) {
			if (typeFilter & (1 << i) && (memoryProps.memoryTypes[i].propertyFlags & properties) == properties) { // typeFilter is a bit field, so checking if bit 'i' is set
				return i;
			}
		}

		assert(false && "ERROR: Could not find suitable memory type");

		return 0;
	}

	void Vulkan::CreateBuffer(VkDeviceSize in_size, VkBufferUsageFlags in_usage, VkMemoryPropertyFlags in_props, VkBuffer& in_buffer, VkDeviceMemory& in_bufferMemory)
	{
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = in_size;
		bufferInfo.usage = in_usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkResult result = vkCreateBuffer(m_device, &bufferInfo, nullptr, &in_buffer);
		assert(result == VK_SUCCESS && "ERROR: vkCreateBuffer() did not return success"); // Check this

		// Buffer is created, but doesn't actually have any memory assigned to it yet
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(m_device, in_buffer, &memRequirements);

		// Allocate the memory
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, in_props);

		result = vkAllocateMemory(m_device, &allocInfo, nullptr, &in_bufferMemory);
		assert(result == VK_SUCCESS && "ERROR: vkAllocateMemory() in Vulkan::CreateBuffer() did not return success");
		result = vkBindBufferMemory(m_device, in_buffer, in_bufferMemory, 0);
		assert(result == VK_SUCCESS && "ERROR: vkBindBufferMemory() did not return success");
	}
	void Vulkan::CopyBuffer(VkBuffer in_srcBuffer, VkBuffer in_dstBuffer, VkDeviceSize in_size)
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommand(m_drawCommandPools[0]);

		VkBufferCopy copyRegion{};
		copyRegion.size = in_size;
		copyRegion.dstOffset = 0;
		vkCmdCopyBuffer(commandBuffer, in_srcBuffer, in_dstBuffer, 1, &copyRegion);

		EndSingleTimeCommand(m_drawCommandPools[0], commandBuffer);
	}
}; // namespace Mega