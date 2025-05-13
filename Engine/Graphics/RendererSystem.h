#pragma once

#include "Engine/Core/Time.h"
#include "Engine/ECS/System.h"
#include "Engine/Animation/OzzMesh.h"
#include "Engine/Graphics/Objects/Model.h"
#include "Engine/Graphics/GraphicComponents.h"

//#define RENDERER_WIREFRAME_DRAWING   uint8_t(64)
//#define RENDERER_PARALLEL_PROJECTION uint8_t(128)

// Forward Declarations
struct GLFWwindow;
namespace Mega
{
	class Engine;
	class Vulkan;
	class WindSystem;
	class AnimationSystem;
	class EulerCamera;
}

namespace Mega
{
	class RendererSystem : public System {
	public:
		friend Engine;

		eMegaResult OnInitialize() override;
		eMegaResult OnDestroy() override;
		eMegaResult OnUpdate(const tTimestep in_dt, Scene* in_pScene) override;

		void DisplayScene(const Scene* in_pScene);
		void DisplayScene(const Scene* in_pScene, const EulerCamera* in_pCamera);
		VertexData LoadOBJ(const tFilePath in_filepath);
		AnimatedVertexData LoadOzzMesh(const ozz::vector<ozz::sample::Mesh>& in_meshes);
		TextureData LoadTexture(const tFilePath in_filepath);

	private:
		void SetWindow(GLFWwindow* in_pWindow) { m_pWindow = in_pWindow; }

		Vulkan* m_pVulkanInstance = nullptr;
		GLFWwindow* m_pWindow = nullptr;

		uint8_t m_bitFieldRenderFlags = 0;
	};
}