#pragma once

#include <GLFW/glfw3.h>

#include "ImGui/imgui.h"
#include "Engine/ECS/ECS.h"
#include "Engine/Wind/Wind.h"
#include "Engine/Core/Core.h"
#include "Engine/Sound/Sound.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Physics/Physics.h"
#include "Engine/Graphics/Graphics.h"
#include "Engine/Animation/Animation.h"

// Forward declaration
struct GLFWwindow;
class Mega::Scene;

namespace Mega
{
	class Engine final
	{
	public:
		// TODO: actually decide what needs to return eMegaResult or not
		// TODO: problem with to ways of getting the registry, who owns it Scene or Engine?
		// TODO: add component callback for animated model so when it gets destroyed the anim sytem deletes it from memory
		// TODO: make components (like transform) either not movable/copyable or also only creatable by Engine
		// TODO: cleanup light component and lightdata jawn
		// TODO: transform component decomposition jawn
		// TODO: camera state, rigid body state, etc should be in the component struct or the corresponding system class? prob components
		// TODO: should game need or be able to access the scene?
		// TODO: make sure systems and scene can only be created by Engine and components can only be created by Engine?
		// TODO: split up Engine class and Enging.h file so the class can be included without every system being included
		// TODO: in the camera class need to seperate GetUp into getting the constant up Y axis and getting the current up vector of the camera based on roll
		// TODO: IK hasReached callback functionality
		friend Vulkan;

		// Engine is not copyable or movable
		Engine(const Engine&) = delete;
		Engine(Engine&&) = delete;
		Engine& operator=(const Engine&) = delete;
		Engine& operator=(Engine&&) = delete;

		inline static Engine* Get() { return s_instance; }

		// ------------- Creation and Update Functions ------------ //
		inline static eMegaResult Initialize() { return Get()->InitializeImpl();  }
		inline static eMegaResult Destroy()    { return Get()->DestroyImpl();     }
		inline static eMegaResult HandleInput() { return Get()->HandleInputImpl(); }
		inline static eMegaResult Update(const tTimestep in_dt) { return Get()->UpdateImpl(in_dt); }
		inline static eMegaResult Display() { return Get()->DisplayImpl(); }

		// ------------ ECS --------------- //
		template<typename tSystem, class... Args> // Allows users to add custom systems
		tSystem* AddSystem(Args&&... in_args)
		{
			MEGA_STATIC_ASSERT(std::is_base_of<System, tSystem>::value, "Type must be descendant of System class to be added to scene");
			MEGA_ASSERT(IsInitialized(), "Adding a system before initializing the engine");

			// Create System
			tSystem* out_system = new tSystem(std::forward<Args>(in_args)...);
			MEGA_ASSERT(out_system, "System could not be allocated");

			out_system->Initialize(this);
			m_pSystems.push_back(out_system);

			return out_system;
		}

		// Adds a child entity to the scene - TODO: this is a hack to inherited entitie's dont have to use Scene to do this
		template<typename tEntityType, class... Args>
		inline static tEntityType* AddChildEntity(Entity* in_owner, Args&&... in_args)
		{
			return Get()->m_pScene->AddEntity<tEntityType>(std::forward<Args>(in_args)...);
		}

		// -------------- Public Asset Loaders ------------------- //
		// Loaders TODO: modulate and make a asset manager?
		static VertexData LoadOBJ(const tFilePath in_filePath); // TODO: string_view?
		static AnimatedMesh LoadAnimatedMesh(const tFilePath in_filePath);
		static AnimatedSkeleton LoadAnimatedSkeleton(const tFilePath in_filePath);
		static Animation LoadAnimation(const tFilePath in_filePath);
		static TextureData LoadTexture(const tFilePath in_filePath);
		static SoundData LoadSound(const tFilePath in_filePath);

		// ---------- Public Getters ---------- //
		static inline Scene* GetScene() { return Get()->m_pScene; } // TODO: remove
		static inline const Input& GetInput() { return Get()->m_input; }
		static inline GLFWwindow* GetAppWindow() { return Get()->m_pAppWindow; }
		static inline tTimestep Runtime() { return Get()->m_dtSum; } // TODO: should be dt sum or real world runtime?

		static inline void SetWindSimulationCenter(const Vec3& in_center) { Get()->m_pWindSystem->SetWindSimulationCenter(in_center); }

		static inline bool ShouldClose() { return glfwWindowShouldClose(GetAppWindow()); }
		static inline bool IsInitialized() { return Get()->m_isInitialized; }

		// ------------- Physics Helpers --------------- // // TODO: should not be necessary
		[[nodiscard]] inline static Vec3 PerformRayTestPosition(const Vec3& in_from, const Vec3& in_to)  { return Get()->m_pPhysicsSystem->PerformRayTestPosition(in_from, in_to);  }
		[[nodiscard]] inline static Vec3 PerformRayTestNormal(const Vec3& in_from, const Vec3& in_to)    { return Get()->m_pPhysicsSystem->PerformRayTestNormal(in_from, in_to);    }
		[[nodiscard]] inline static bool PerformRayTestCollision(const Vec3& in_from, const Vec3& in_to) { return Get()->m_pPhysicsSystem->PerformRayTestCollision(in_from, in_to); }

	private:
		static Engine* s_instance;

		// ---------- Implemented Functions ------------ //
		Engine() = default;
		eMegaResult InitializeImpl();
		eMegaResult DestroyImpl();
		eMegaResult HandleInputImpl();
		eMegaResult UpdateImpl(const tTimestep in_dt);
		eMegaResult DisplayImpl();
		Scene* CreateSceneImpl();

		// --------------- Systems -------------- //
		std::vector<System*> m_pSystems;
		RendererSystem* m_pRendererSystem = nullptr;
		CameraSystem* m_pCameraSystem = nullptr;
		PhysicsSystem* m_pPhysicsSystem = nullptr;
		AnimationSystem* m_pAnimationSystem = nullptr;
		WindSystem* m_pWindSystem = nullptr;
		SoundSystem* m_pSoundSystem = nullptr;

		// ------------ Member Variables ---------- //
		Input m_input;
		tTimestep m_dtSum = 0;

		bool m_isInitialized = false;
		Scene* m_pScene = nullptr;
		GLFWwindow* m_pAppWindow = nullptr;
	};
} // namespace Mega