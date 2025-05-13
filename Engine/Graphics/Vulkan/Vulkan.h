#pragma once

#include <vector>

#include <GLM/common.hpp>
#include <GLM/gtx/hash.hpp>

#include "Engine/Core/Math/Vector.h"
#include "Engine/Graphics/Objects/Vertex.h"
#include "Engine/Camera/EulerCamera.h"
#include "Engine/Animation/OzzMesh.h"
#include "Engine/Core/Core.h"

#include "VulkanInclude.h"
#include "VulkanObjects.h"
#include "VulkanImgui.h"
#include "ShaderCompiler.h"

#define INDEX_TYPE uint32_t
#define MTL_BASE_DIR "Assets/Models"

namespace Mega
{
	class Scene;
	class RendererSystem;
	struct Light;
	struct VertexData;
	struct AnimatedVertexData;
	struct TextureData;
}

namespace std {
	template<> struct hash<Mega::Vertex> {
		size_t operator()(Mega::Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<Mega::Vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<Mega::Vec2>()(vertex.texCoord) << 1);
		}
	};
}

namespace Mega
{
	enum class eVulkanInitState {
		Created,
		Initialzed,
		Destroyed
	};

	class Vulkan {
	private:
		friend RendererSystem;
		friend ImguiObject;

		VertexData* m_pBoxVertexData;

		void Initialize(RendererSystem* in_pRenderer, GLFWwindow* in_pWindow);
		void Destroy();
		void CleanupSwapchain(VkSwapchainKHR* in_pSwapchain);

		void DrawFrame(const Scene* in_pScene);

		void SetCamera(const EulerCamera* in_pCamera);

		void LoadVertexData(const char* in_objPath, VertexData* in_pVertexData, const char* in_MTLDir = MTL_BASE_DIR);
		void LoadOzzMeshData(const ozz::vector<ozz::sample::Mesh>& in_meshes, AnimatedVertexData* in_pVertexData);
		void LoadTextureData(const char* in_texPath, TextureData* in_pTextureData);

		void CreateIndexBuffer(std::vector<INDEX_TYPE>& in_indices, VkBuffer& in_buffer, VkDeviceMemory& in_memory);
		template<typename T>
		void CreateVertexBuffer(std::vector<T>& in_vertices, VkBuffer& in_buffer, VkDeviceMemory& in_memory)
		{
			std::cout << "Creating vertex buffer..." << std::endl;

			VkDeviceSize bufferSize = sizeof(in_vertices[0]) * in_vertices.size();

			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

			// Map the veretx data to the buffer
			void* data;
			vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
			memcpy(data, in_vertices.data(), (size_t)bufferSize);
			vkUnmapMemory(m_device, stagingBufferMemory);

			CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, in_buffer, in_memory);

			CopyBuffer(stagingBuffer, in_buffer, bufferSize);

			vkDestroyBuffer(m_device, stagingBuffer, nullptr);
			vkFreeMemory(m_device, stagingBufferMemory, nullptr);
		}
		void UpdateLoadedIndexData(std::vector<INDEX_TYPE>& in_indices, VkBuffer& in_buffer, VkDeviceMemory& in_memory);
		template<typename T>
		void UpdateLoadedVertexData(std::vector<T>& in_vertices, VkBuffer& in_buffer, VkDeviceMemory& in_memory)
		{
			VkDeviceSize bufferSize = sizeof(in_vertices[0]) * in_vertices.size();

			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

			// Map the veretx data to the buffer
			void* data;
			vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
			memcpy(data, in_vertices.data(), (size_t)bufferSize);
			vkUnmapMemory(m_device, stagingBufferMemory);

			vkDestroyBuffer(m_device, in_buffer, nullptr);
			vkFreeMemory(m_device, in_memory, nullptr);
			CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, in_buffer, in_memory);

			CopyBuffer(stagingBuffer, in_buffer, bufferSize);

			vkDestroyBuffer(m_device, stagingBuffer, nullptr);
			vkFreeMemory(m_device, stagingBufferMemory, nullptr);
		}
		void UpdateLoadedTextureData();
		void UpdateUniformBuffer(uint32_t in_imageIndex, const Scene* in_pScene);

		template<typename T>
		struct VertexBuffer
		{
			friend Vulkan;
			static void Create(VertexBuffer<T>& in_vib, Vulkan* in_pVulkanInstance)
			{
				assert(in_pVulkanInstance != nullptr);
				in_vib.pVulkanInstance = in_pVulkanInstance;

				in_pVulkanInstance->CreateVertexBuffer<T>(in_vib.vertices, in_vib.vertexBuffer, in_vib.vertexBufferMemory);

				in_vib.isCreated = true;
			}
			static void Destroy(VertexBuffer<T>& in_vib)
			{
				assert(in_vib.pVulkanInstance != nullptr);
				vkDestroyBuffer(in_vib.pVulkanInstance->m_device, in_vib.vertexBuffer, nullptr);
				vkFreeMemory(in_vib.pVulkanInstance->m_device, in_vib.vertexBufferMemory, nullptr);
			}
			static void UpdateData(VertexBuffer<T>& in_vib)
			{
				assert(in_vib.pVulkanInstance != nullptr);

				in_vib.pVulkanInstance->UpdateLoadedVertexData<T>(in_vib.vertices, in_vib.vertexBuffer, in_vib.vertexBufferMemory);
			}
			static void Bind(VertexBuffer<T>& in_vib, VkCommandBuffer* in_cb)
			{
				assert(in_vib.pVulkanInstance != nullptr);
				assert(in_cb != VK_NULL_HANDLE && "Bind vertex/index buffer to a NULL command buffer");

				VkBuffer vertexBuffers[] = { in_vib.vertexBuffer };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(*in_cb, 0, 1, vertexBuffers, offsets);
			}
			static bool IsCreated(VertexBuffer<T>& in_vib)
			{
				return in_vib.isCreated;
			}

		private:
			bool isCreated = false;
			Vulkan* pVulkanInstance = nullptr;

			std::vector<T> vertices;
			VkBuffer vertexBuffer;
			VkDeviceMemory vertexBufferMemory; // Handle to vertex buffer
		};
		struct IndexBuffer
		{
			friend Vulkan;
			static void Create(IndexBuffer& in_ib, Vulkan* in_pVulkanInstance)
			{
				assert(in_pVulkanInstance != nullptr);
				in_ib.pVulkanInstance = in_pVulkanInstance;

				in_pVulkanInstance->CreateIndexBuffer(in_ib.indices, in_ib.indexBuffer, in_ib.indexBufferMemory);
			}
			static void Destroy(IndexBuffer& in_ib)
			{
				assert(in_ib.pVulkanInstance != nullptr);

				vkDestroyBuffer(in_ib.pVulkanInstance->m_device, in_ib.indexBuffer, nullptr);
				vkFreeMemory(in_ib.pVulkanInstance->m_device, in_ib.indexBufferMemory, nullptr);
			}
			static void UpdateData(IndexBuffer& in_ib)
			{
				assert(in_ib.pVulkanInstance != nullptr);
				in_ib.pVulkanInstance->UpdateLoadedIndexData(in_ib.indices, in_ib.indexBuffer, in_ib.indexBufferMemory);
			}
			static void Bind(IndexBuffer& in_ib, VkCommandBuffer* in_cb)
			{
				assert(in_ib.pVulkanInstance != nullptr);
				assert(in_cb != VK_NULL_HANDLE && "Bind vertex/index buffer to a NULL command buffer");
				vkCmdBindIndexBuffer(*in_cb, in_ib.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
			}

		private:
			Vulkan* pVulkanInstance = nullptr;

			std::vector<INDEX_TYPE> indices;
			VkBuffer indexBuffer;
			VkDeviceMemory indexBufferMemory;
		};

		// Vertex Data
		VertexBuffer<Mega::Vertex> m_vertexBuffer;
		VertexBuffer<Mega::AnimatedVertex> m_animatedVertexBuffer;
		IndexBuffer m_indexBuffer;

	private:
		struct Pipeline
		{static void Bind(Pipeline& in_pipeline, VkCommandBuffer* in_pCB, int in_imageIndex);

			Vulkan* pVulkanInstance = nullptr;
			VkPipeline graphicsPipeline = VK_NULL_HANDLE;
			VkPipelineLayout layout = VK_NULL_HANDLE;
			VkShaderModule vertexShader = VK_NULL_HANDLE;
			VkShaderModule fragmentShader = VK_NULL_HANDLE;

			const char* name = "Null";
			
		};

		struct ComputePipeline
		{

			static void Destroy(Vulkan* in_pVulkanInstance, ComputePipeline in_pipeline)
			{
				vkDestroyShaderModule(in_pVulkanInstance->m_device, in_pipeline.computeShader, nullptr);
				vkDestroyPipelineLayout(in_pVulkanInstance->m_device, in_pipeline.layout, nullptr);
			}
			
			VkPipeline pipeline = VK_NULL_HANDLE;
			VkPipelineLayout layout = VK_NULL_HANDLE;
			VkShaderModule computeShader = VK_NULL_HANDLE;
		};
		
		void CreateComputePipeline(const char* in_computeShader, ComputePipeline* in_pPipeline, uint32_t in_pushSize, bool in_isBloom = false)
		{
			in_pPipeline->computeShader = ShaderHelper::CompileAndCreateShader(m_device, in_computeShader + std::string(".comp"), in_computeShader + std::string(".spv"));

			VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
			computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			computeShaderStageInfo.module = in_pPipeline->computeShader;
			computeShaderStageInfo.pName = "main";

			// Layout
			std::vector<VkDescriptorSetLayout> descriptorLayouts;
			VkPipelineLayoutCreateInfo pipelineLayoutInfo{};

			if (in_isBloom)
			{
				descriptorLayouts.resize(1);
				descriptorLayouts = {
					m_descriptorSetLayoutBloomCompute,
				};

				pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
				pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorLayouts.size());
				pipelineLayoutInfo.pSetLayouts = descriptorLayouts.data();
			}
			else
			{
				descriptorLayouts.resize(2);
				descriptorLayouts = {
					m_descriptorSetLayoutGrassCompute,
					m_descriptorSetLayoutWindData,
				};

				pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
				pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorLayouts.size());
				pipelineLayoutInfo.pSetLayouts = descriptorLayouts.data();
			}

			if (in_pushSize > 0)
			{
				VkPushConstantRange pushConstantRange{};
				pushConstantRange.offset = 0;
				pushConstantRange.size = (uint32_t)in_pushSize;
				pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

				pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
				pipelineLayoutInfo.pushConstantRangeCount = 1;
			}

			if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &in_pPipeline->layout) != VK_SUCCESS) {
				throw std::runtime_error("failed to create compute pipeline layout!");
			}

			// Pipeline
			VkComputePipelineCreateInfo pipelineInfo{};
			pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipelineInfo.layout = in_pPipeline->layout;
			pipelineInfo.stage = computeShaderStageInfo;

			if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &in_pPipeline->layout) != VK_SUCCESS) {
				throw std::runtime_error("failed to create compute pipeline layout!");
			}

			vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &in_pPipeline->pipeline);
		}
		void DestroyPipeline(Pipeline& in_pipeline);

		// Setup
		void CreateInstance();
		bool CheckValidationLayerSupport();
		bool CheckGLFWExtensionSupport();

		void CreateSceneImages();
		void DestroySceneImages();

		void CreateWin32SurfaceKHR(GLFWwindow* in_pWindow, VkSurfaceKHR& in_surface);

		void PickPhysicalDevice(VkPhysicalDevice& in_device);
		QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice in_device, VkSurfaceKHR in_surface);
		bool IsPhysicalDeviceSuitable(const VkPhysicalDevice& in_device, const VkSurfaceKHR& in_surface);
		bool CheckPhysicalDeviceExtensionSupport(const VkPhysicalDevice& in_device);
		SwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice& in_device, const VkSurfaceKHR& in_surface);
		VkSampleCountFlagBits GetMaxUsableSampleCount(const VkPhysicalDevice& in_device);

		void CreateLogicalDevice(VkDevice& in_device);

		void CreateSwapchain(GLFWwindow* in_pWindow, const VkSurfaceKHR in_surface, VkSwapchainKHR& in_swapchain);

		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& in_availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& in_availableModes);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& in_capabilities, GLFWwindow* in_pWindow);

		void RetrieveSwapchainImages(std::vector<VkImage>& in_images, VkSwapchainKHR in_swapchain);
		void CreateSwapchainImageViews(std::vector<VkImageView>& in_imageViews, const std::vector<VkImage>& in_images);

		void CreateDescriptorSetLayout(const VkDevice in_device, VkDescriptorSetLayout& in_descriptorSetLayout);
		void CreateRenderPass();
		void CreateRenderPassPost();
		Pipeline CreateShadowMapPipeline(const char* in_vertShader, const uint32_t in_pushSize, const bool in_animated = false);
		Pipeline CreateGraphicsPipeline(const char* in_vertShader, const char* in_fragShader, const size_t in_pushSize, const char* in_name, bool in_animated = false, bool is_grass = false);

		void CreateBloom();
		void DestroyBloom();
		void CreateFramebuffers(std::vector<VkFramebuffer>& in_swapchainFramebuffers);

		void CreateDrawCommandPools(std::vector<VkCommandPool>& in_pools);
		void CreateDrawCommands(std::vector<VkCommandBuffer>& in_buffers, std::vector<VkCommandPool>& in_pools);
		void AllocateCommandBuffer(VkCommandBuffer& in_buffer, const VkCommandPool& in_pool);

		VkCommandBuffer BeginSingleTimeCommand(const VkCommandPool& in_pool);
		void EndSingleTimeCommand(const VkCommandPool& in_pool, VkCommandBuffer in_command);

		void CreateDepthResources(ImageObject& in_depthObject);
		void CreateColorResources(ImageObject& in_colorObject); // Anti aliasing

		VkFormat FindSupportedFormat(const std::vector<VkFormat>& in_candidates, VkImageTiling in_tiling, VkFormatFeatureFlags in_features);
		VkFormat FindDepthFormat();
		bool HasStencilComponent(VkFormat format);

		void CreateDescriptorPool();
		void CreateDescriptorPoolPost();
		void CreateDescriptorSets();
		void CreateDescriptorSetsPost();
		void UpdateDescriptorSets();
		void UpdateDescriptorSetsPost();

		void CreateUniformBuffers();

		void CreateSyncObjects();

		// Other
		void CreateTextureSampler(VkSampler in_sampler);
		void CreateTextureSamplerPost();
		void CreateTextureImage(ImageObject& in_imageObject, const char* in_filename);

		void CreateImageObject(VkImageCreateInfo& in_info, VkMemoryPropertyFlags in_properties, VkImage& in_image, VkDeviceMemory& in_imageMemory);
		void ChangeImageLayout(VkImage& in_image, VkFormat in_format, VkImageLayout in_old, VkImageLayout in_new);
		void CopyBufferToImage(VkBuffer& in_buffer, VkImage& in_image, uint32_t in_width, uint32_t in_height);
		void CreateImageView(VkImageView& in_view, VkFormat in_format, VkImageAspectFlags in_aspectFlags, VkImage& in_image);
		void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, \
			VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

		// Helper functions
		glm::vec3 NormalizeRGB(const glm::vec3& c);

		uint32_t FindMemoryType(uint32_t in_typeFilter, VkMemoryPropertyFlags in_properties);

		void CreateBuffer(VkDeviceSize in_size, VkBufferUsageFlags in_usage, VkMemoryPropertyFlags in_props, VkBuffer& in_buffer, VkDeviceMemory& in_bufferMemory);
		void CopyBuffer(VkBuffer in_srcBuffer, VkBuffer in_dstBuffer, VkDeviceSize in_size);

		int GetRuntime() const { return (int)(Mega::Time() - m_startTime); }

	public:
		bool m_frameBufferResized = false;

	private:
		Mega::tTime m_startTime;
		// Instance member variables
		static Vulkan* staticInstance;
		eVulkanInitState m_state = eVulkanInitState::Created;

		// Other member variables
		RendererSystem* m_pRenderer;
		const EulerCamera* m_pCamera = nullptr;

		// GLFW member variables
		GLFWwindow* m_pWindow;

		// ImGui
		ImguiObject m_imguiObject;

		VkDescriptorPool m_imguiDescriptorPool;
		ImGui_ImplVulkanH_Window m_imguiWindow;
		std::vector<ImGui_ImplVulkanH_Frame> m_imguiFramebuffers;

		// Vulkan member variables
		VkInstance m_instance = nullptr;
		VkSurfaceKHR m_surface = nullptr;

		VkPhysicalDevice m_physicalDevice = nullptr;
		VkPhysicalDeviceProperties m_physicalDeviceProperties;
		VkPhysicalDeviceFeatures m_physicalDeviceFeatures;

		VkDevice m_device = nullptr;

		// Pipelines
		Pipeline m_mainPipeline;
		Pipeline m_animationPipeline;
		Pipeline m_waterPipeline;
		Pipeline m_grassPipeline;

		Pipeline m_shadowMapPipeline;
		Pipeline m_shadowMapAnimationPipeline;

		ComputePipeline m_grassComputePipeline;
		ComputePipeline m_bloomComputePipeline;

		VkRenderPass m_renderPass;
		VkRenderPass m_renderPassPost;
		VkRenderPass m_renderPassShadowMap;
		VkRenderPass m_imguiRenderPass;

		VkSwapchainKHR m_swapchain;

		std::vector<ImageObject> m_sceneImages;

		VkSurfaceFormatKHR m_surfaceFormat;
		VkPresentModeKHR m_presentMode;
		VkExtent2D m_swapchainExtent;

		std::vector<VkImage> m_swapchainImages;
		std::vector<VkImageView> m_swapchainImageViews;
		std::vector<VkFramebuffer> m_swapchainFramebuffers;

		std::vector<VkFramebuffer> m_shadowMapFramebuffers;

		std::vector<VkCommandPool> m_drawCommandPools;
		std::vector<VkCommandBuffer> m_drawCommandBuffers;
		std::vector<VkCommandBuffer> m_shadowMapCommandBuffers;
		std::vector<VkCommandBuffer> m_grassComputeCommandBuffers;
		std::vector<VkCommandBuffer> m_bloomComputeCommandBuffers;

		VkDescriptorPool m_descriptorPool;
		VkDescriptorSetLayout m_descriptorSetLayout;
		std::vector<VkDescriptorSet> m_descriptorSets;

		VkDescriptorSetLayout m_descriptorSetLayoutAnimation;
		std::vector<VkDescriptorSet> m_descriptorSetsAnimation;

		VkDescriptorSetLayout m_descriptorSetLayoutWindData;
		std::vector<VkDescriptorSet> m_descriptorSetsWindData;

		VkDescriptorSetLayout m_descriptorSetLayoutGrassCompute;
		std::vector<VkDescriptorSet> m_descriptorSetsGrassCompute;

		VkDescriptorSetLayout m_descriptorSetLayoutBloomCompute;
		std::vector<VkDescriptorSet> m_descriptorSetsBloomCompute;

		std::vector<VkBuffer> m_uniformBuffersVert;
		std::vector<VkDeviceMemory> m_uniformBuffersMemoryVert;
		std::vector<VkBuffer> m_uniformBuffersFrag;
		std::vector<VkDeviceMemory> m_uniformBuffersMemoryFrag;
		std::vector<VkBuffer> m_ssboAnimation;
		std::vector<VkDeviceMemory> m_ssboAnimationMemory;

		VkBuffer m_ssboGrassCompute;
		VkDeviceMemory m_ssboGrassComputeMemory;
		VkBuffer m_ssboGrassLowCompute;
		VkDeviceMemory m_ssboGrassLowComputeMemory;
		VkBuffer m_ssboGrassFlowerCompute;
		VkDeviceMemory m_ssboGrassFlowerComputeMemory;

		std::vector<VkBuffer> m_ssboWindData;
		std::vector<VkDeviceMemory> m_ssboWindDataMemory;

		void* m_windDataStagingBufferMap = nullptr; // Pointer to mapped memory
		VkDeviceSize m_windDataBufferSize;
		VkBuffer m_windDataStagingBuffer;
		VkDeviceMemory m_windDataStagingBufferMemory;

		// TODO: do i need 3 of these buffers
		std::vector<VkBuffer> m_uboGrassCompute;
		std::vector<VkDeviceMemory> m_uboGrassComputeMemory;

		std::vector<glm::mat4> m_ssboAnimationMats;
		void* m_animStagingBufferMap = nullptr; // Pointer to mapped memory
		VkDeviceSize m_animBufferSize;
		VkBuffer m_animStagingBuffer;
		VkDeviceMemory m_animStagingBufferMemory;


		size_t m_currentFrame = 0;
		const int MAX_FRAMES_IN_FLIGHT = 2;
		std::vector<VkSemaphore> m_imageAvailableSemaphores;
		std::vector<VkSemaphore> m_renderFinishedSemaphores;
		std::vector<VkSemaphore> m_shadowMapFinishedSemaphores;
		std::vector<VkSemaphore> m_bloomFinishedSemaphores;

		std::vector<VkFence> m_inFlightFences;
		std::vector<VkFence> m_imagesInFlight;

		std::vector<VkFence> m_computeInFlightFences;
		std::vector<VkSemaphore> m_computeFinishedSemaphores;

		QueueFamilyIndices m_queueFamilyIndices;
		VkQueue m_graphicsQueue;
		VkQueue m_presentQueue;
		VkQueue m_computeQueue;

		std::vector<const char*> m_validationLayers =
		{
			"VK_LAYER_KHRONOS_validation",
			"VK_LAYER_NV_optimus",
			"VK_LAYER_RENDERDOC_Capture",
			"VK_LAYER_VALVE_steam_overlay",
			"VK_LAYER_VALVE_steam_fossilize",
			"VK_LAYER_EOS_Overlay",
			"VK_LAYER_EOS_Overlay",
			"VK_LAYER_LUNARG_api_dump",
			"VK_LAYER_LUNARG_gfxreconstruct",
			"VK_LAYER_KHRONOS_synchronization2",
			"VK_LAYER_KHRONOS_validation",
			"VK_LAYER_LUNARG_monitor",
			"VK_LAYER_LUNARG_screenshot",
			"VK_LAYER_KHRONOS_profiles"
		};
		std::vector<const char*> m_physicalDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
		ImageObject m_colorObject; // Multi sampling

		std::vector<ImageObject> m_bloomImages;
		ImageObject m_bloomInput;

		// Z-Buffering
		ImageObject m_depthObject;
		ImageObject m_shadowMapObject;

		VkQueryPool m_queryPool;

		// Assets
		VkSampler m_sampler;
		VkSampler m_samplerPost;

		ImageObject m_grassComputeTexture;
		ImageObject m_perlinNoiseTexture;

		std::vector<ImageObject> m_textures;
		uint32_t m_loadedTextureCount = 0;
	};
}