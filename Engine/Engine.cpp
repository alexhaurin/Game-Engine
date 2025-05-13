#include "Engine.h"

#include <GLFW/glfw3.h>
#include <ctime>
#include "ImGui/Graphics/imgui_impl_glfw.h"
#include "ImGui/Graphics/imgui_impl_vulkan.h"

#include "Engine/Scene/Scene.h"
#include "Engine/Graphics/RendererSystem.h"

Mega::Engine* Mega::Engine::s_instance = new Mega::Engine();

namespace Mega
{
	eMegaResult Engine::InitializeImpl()
	{
		srand((uint32_t)Time());

		// Initialize GLFW and create our application window
		glfwInit();

		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Telling it not to create an OpenGL context
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // For now telling GLFW we dont want the window to be resized
		glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
		
		glfwWindowHint(GLFW_RED_BITS, mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

		// Create and position before showing
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		m_pAppWindow = glfwCreateWindow(mode->width, mode->height, "SuperUltraMega3D", nullptr, nullptr);

		int x, y; glfwGetWindowPos(m_pAppWindow, &x, &y);
		glfwSetWindowPos(m_pAppWindow, 0, y);
		glfwShowWindow(m_pAppWindow);

		// Input settings
		glfwSetInputMode(m_pAppWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Cursor hidden and movement unrestricted

		// Setup systems and the default scene
		m_pScene = new Scene();
		m_pScene->Initialize();

		m_pPhysicsSystem = new PhysicsSystem;
		m_pPhysicsSystem->Initialize();
		m_pSystems.push_back(m_pPhysicsSystem);

		m_pAnimationSystem = new AnimationSystem;
		m_pAnimationSystem->Initialize();
		m_pSystems.push_back(m_pAnimationSystem);

		m_pWindSystem = new WindSystem;
		m_pWindSystem->Initialize();
		m_pSystems.push_back(m_pWindSystem);

		m_pSoundSystem = new SoundSystem;
		m_pSoundSystem->Initialize();
		m_pSystems.push_back(m_pSoundSystem);

		m_pCameraSystem = new CameraSystem;
		m_pCameraSystem->Initialize();
		m_pSystems.push_back(m_pCameraSystem);

		m_pRendererSystem = new RendererSystem;
		m_pRendererSystem->Initialize();
		m_pSystems.push_back(m_pRendererSystem);

		m_isInitialized = true;

		return eMegaResult::SUCCESS;
	}
	eMegaResult Engine::DestroyImpl()
	{
		// Clenup our world
		m_pScene->Destroy();
		delete m_pScene;

		// Destroy and delete all systems
		for (System* pSystem : m_pSystems)
		{
			pSystem->Destroy();
		}

		for (System* pSystem : m_pSystems)
		{
			delete pSystem;
		}

		// Delete our application window
		glfwDestroyWindow(m_pAppWindow);
		glfwTerminate();

		return eMegaResult::SUCCESS;
	}

	Scene* Engine::CreateSceneImpl()
	{
		m_pScene = new Scene;
		m_pScene->Initialize();

		return m_pScene;
	}

	eMegaResult Engine::HandleInputImpl()
	{
		glfwPollEvents();

		m_input.keyW = (glfwGetKey(m_pAppWindow, GLFW_KEY_W) == GLFW_PRESS);
		m_input.keyA = (glfwGetKey(m_pAppWindow, GLFW_KEY_A) == GLFW_PRESS);
		m_input.keyS = (glfwGetKey(m_pAppWindow, GLFW_KEY_S) == GLFW_PRESS);
		m_input.keyD = (glfwGetKey(m_pAppWindow, GLFW_KEY_D) == GLFW_PRESS);
		m_input.keySpace = (glfwGetKey(m_pAppWindow, GLFW_KEY_SPACE) == GLFW_PRESS);
		m_input.keyLShift = (glfwGetKey(m_pAppWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
		m_input.keyUp = (glfwGetKey(m_pAppWindow, GLFW_KEY_UP) == GLFW_PRESS);
		m_input.keyDown = (glfwGetKey(m_pAppWindow, GLFW_KEY_DOWN) == GLFW_PRESS);
		m_input.keyRight = (glfwGetKey(m_pAppWindow, GLFW_KEY_RIGHT) == GLFW_PRESS);
		m_input.keyLeft = (glfwGetKey(m_pAppWindow, GLFW_KEY_LEFT) == GLFW_PRESS);
		m_input.keyTab = (glfwGetKey(m_pAppWindow, GLFW_KEY_TAB) == GLFW_PRESS);

		m_input.lastMousePosX = m_input.mousePosX;
		m_input.lastMousePosY = m_input.mousePosY;
		glfwGetCursorPos(m_pAppWindow, &m_input.mousePosX, &m_input.mousePosY);
		m_input.mouseMovementX = m_input.mousePosX - m_input.lastMousePosX;
		m_input.mouseMovementY = m_input.mousePosY - m_input.lastMousePosY;

		if (glfwGetKey(m_pAppWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) { glfwSetInputMode(m_pAppWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL); }

		return eMegaResult::SUCCESS;
	}

	eMegaResult Engine::UpdateImpl(const tTimestep in_dt)
	{
		m_dtSum += in_dt;

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Pre-physics update
		m_pScene->Update(in_dt);
		m_pPhysicsSystem->OnUpdate(in_dt, m_pScene);

		// Post-physics update
		m_pScene->UpdatePost(in_dt);
		for (System* pSystem : m_pSystems)
		{
			if (pSystem == m_pPhysicsSystem) { continue; }
			pSystem->Update(in_dt, m_pScene);
		}

		return eMegaResult::SUCCESS;
	}

	eMegaResult Engine::DisplayImpl()
	{
		m_pRendererSystem->DisplayScene(m_pScene, m_pCameraSystem->GetActiveCamera());

		return eMegaResult::SUCCESS;
	} 

	// ------------------ Asset Loaders -------------------- //
	VertexData Engine::LoadOBJ(const tFilePath in_filePath)
	{
		//MEGA_ASSERT(IsInitialized(), "Trying to load obj while scene is not initialized");
		return Get()->m_pRendererSystem->LoadOBJ(in_filePath);
	}
	AnimatedMesh Engine::LoadAnimatedMesh(const tFilePath in_filePath)
	{
		SKINNING_MAT_INDEX_T skinningMatsIndiceStart = (int32_t)Get()->m_pAnimationSystem->m_glmModels.size();

		AnimatedMesh out_mesh = Get()->m_pAnimationSystem->LoadAnimatedMesh(in_filePath);

		out_mesh.skinningMatsIndiceStart = skinningMatsIndiceStart;
		out_mesh.vertexData = Get()->m_pRendererSystem->LoadOzzMesh(Get()->m_pAnimationSystem->m_meshes[out_mesh.meshIndex]);

		return out_mesh;
	}
	AnimatedSkeleton Engine::LoadAnimatedSkeleton(const tFilePath in_filePath)
	{
		return Get()->m_pAnimationSystem->LoadAnimatedSkeleton(in_filePath);
	}
	Animation Engine::LoadAnimation(const tFilePath in_filePath)
	{
		return Get()->m_pAnimationSystem->LoadAnimation(in_filePath);
	}
	TextureData Engine::LoadTexture(const tFilePath in_filePath)
	{
		return Get()->m_pRendererSystem->LoadTexture(in_filePath);
	}
	SoundData Engine::LoadSound(const tFilePath in_filePath)
	{
		return Get()->m_pSoundSystem->LoadSound(in_filePath);
	}

} // namespace Mega