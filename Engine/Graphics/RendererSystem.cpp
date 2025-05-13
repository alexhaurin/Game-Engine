#include "RendererSystem.h"

#include <memory>
#include <cstdint>
#include <iostream>

#include "Engine/Engine.h"
#include "Engine/Core/Debug.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Graphics/Vulkan/Vulkan.h"

namespace Mega
{
	eMegaResult RendererSystem::OnInitialize()
	{
		SetWindow(Engine::GetAppWindow());
		MEGA_ASSERT(m_pWindow != nullptr, "Initializing RendererSystem without a window; Set scene first");
		// Initialize Graphics
		std::cout << "Initializing RendererSystem..." << std::endl;

		m_pVulkanInstance = new Vulkan;
		m_pVulkanInstance->Initialize(this, m_pWindow);

		return eMegaResult::SUCCESS;
	}

	eMegaResult RendererSystem::OnDestroy()
	{
		// Destroy our Vulkan instance
		m_pVulkanInstance->Destroy();
		delete m_pVulkanInstance;

		return eMegaResult::SUCCESS;
	}

	eMegaResult RendererSystem::OnUpdate(const tTimestep in_dt, Scene* in_pScene)
	{
		return eMegaResult::SUCCESS;
	}

	void RendererSystem::DisplayScene(const Scene* in_pScene) {
		MEGA_ASSERT(IsInitialized(), "Trying to display scene with unitialized renderer");
		MEGA_ASSERT(in_pScene != nullptr, "Trying to display null scene");

		m_pVulkanInstance->DrawFrame(in_pScene);
	}

	void RendererSystem::DisplayScene(const Scene* in_pScene, const EulerCamera* in_pCamera) {
		MEGA_ASSERT(IsInitialized(), "Trying to display scene with unitialized renderer");
		MEGA_ASSERT(in_pScene != nullptr, "Trying to display null scene");

		m_pVulkanInstance->SetCamera(in_pCamera);
		m_pVulkanInstance->DrawFrame(in_pScene);
	}

	VertexData RendererSystem::LoadOBJ(const tFilePath in_filepath)
	{
		MEGA_ASSERT(IsInitialized(), "Trying to load obj while renderer is not initialized");

		VertexData out_vertexData;
		m_pVulkanInstance->LoadVertexData(in_filepath.data(), &out_vertexData);

		Vulkan::VertexBuffer<Vertex>::UpdateData(m_pVulkanInstance->m_vertexBuffer);
		Vulkan::IndexBuffer::UpdateData(m_pVulkanInstance->m_indexBuffer);

		return out_vertexData;
	}

	AnimatedVertexData RendererSystem::LoadOzzMesh(const ozz::vector<ozz::sample::Mesh>& in_meshes)
	{
		MEGA_ASSERT(IsInitialized(), "Trying to load obj while renderer is not initialized");

		AnimatedVertexData out_vertexData;
		m_pVulkanInstance->LoadOzzMeshData(in_meshes, &out_vertexData);

		if (!Vulkan::VertexBuffer<AnimatedVertex>::IsCreated(m_pVulkanInstance->m_animatedVertexBuffer))
		{
			Vulkan::VertexBuffer<AnimatedVertex>::Create(m_pVulkanInstance->m_animatedVertexBuffer, m_pVulkanInstance);
		}
		Vulkan::VertexBuffer<AnimatedVertex>::UpdateData(m_pVulkanInstance->m_animatedVertexBuffer);
		Vulkan::IndexBuffer::UpdateData(m_pVulkanInstance->m_indexBuffer);

		return out_vertexData;
	}

	TextureData RendererSystem::LoadTexture(const tFilePath in_filepath)
	{
		MEGA_ASSERT(IsInitialized(), "Trying to load texture while renderer is not initialized");

		TextureData out_textureData;
		m_pVulkanInstance->LoadTextureData(in_filepath.data(), &out_textureData);

		return out_textureData;
	}
};