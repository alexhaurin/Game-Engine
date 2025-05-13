#pragma once

#include <entt/entt.hpp>
#include "Engine/Core/Core.h"

#ifdef NDEBUG
#define CHECK_SYSTEM_RESULT(func) \
		eMegaResult res = func; \
		MEGA_ASSERT(res == eMegaResult::Success, "System function call did not return success");
#else
#define CHECK_SYSTEM_RESULT(func) func
#endif

// Forward Declarations
namespace Mega
{
	class Scene;
	class Engine;
}

namespace Mega
{
	enum class eSystemState
	{
		Created = 0,
		Initialized = 1,
		Destroyed = -1,
	};

	// A guard used to ensure systems are properly initialized and destroyed and
	// also controls system updating.
	class System
	{
	public:
		friend Engine;

	protected:
		// Systems are only creatable and destroyable by Engine and inherited systems
		System() = default;
		~System() { MEGA_ASSERT(IsDestroyed(), "Deleting system that has not been properly destoyed"); } // TODO: can be private because cleanup handled by OnDestroy()?

		// State helpers
		bool IsCreated()     const { return m_state == eSystemState::Created; }
		bool IsInitialized() const { return m_state == eSystemState::Initialized; }
		bool IsDestroyed()   const { return m_state == eSystemState::Destroyed; }

		// Init, Destroy and Update functions that allow per system implementation
		virtual eMegaResult OnInitialize() = 0;
		virtual eMegaResult OnDestroy() = 0;
		virtual eMegaResult OnUpdate(const tTimestep in_dt, Scene* in_pScene) = 0;

		eSystemState GetState() const { return m_state; }

	private:
		// Systems are not movable or copyable
		System(const System & in_system) = delete;
		System(System&&) = delete;
		System& operator=(const System & in_system) = delete;
		System& operator=(System&&) = delete;

		// Init, Destroy and Update functions that are handled by SystemGuard
		inline void Initialize()
		{
			CHECK_SYSTEM_RESULT(OnInitialize());
			SetStateInitialized();
		}
		inline void Destroy()
		{
			CHECK_SYSTEM_RESULT(OnDestroy());

			SetStateDestroyed();
		}
		inline void Update(const tTimestep in_dt, Scene * in_pScene)
		{
			MEGA_ASSERT(IsInitialized(), "Trying to update unitialized system");

			CHECK_SYSTEM_RESULT(OnUpdate(in_dt, in_pScene));
		}

		inline void SetStateInitialized()
		{
			MEGA_ASSERT(IsCreated(), "Initialiazing from state other than created prob doin something weird");
			m_state = eSystemState::Initialized;
		}
		inline void SetStateDestroyed()
		{
			MEGA_ASSERT(IsInitialized(), "Destroying unitialized object");
			m_state = eSystemState::Destroyed;
		}

		eSystemState m_state = eSystemState::Created;
	};
} // namespace Mega

#undef CHECK_SYSTEM_RESULT