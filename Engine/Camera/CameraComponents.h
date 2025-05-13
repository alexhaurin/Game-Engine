#pragma once

#include "Engine/ECS/Components.h"

namespace Mega
{
	namespace Component
	{
		struct CameraTarget : public ComponentBase
		{
		public:
			// A trick to keep the movement enums and their
			// string counterparts consistent
			#define REGISTER_ENUM(state) state,
			enum class eCameraState : int32_t
			{
				#include "CameraStates.h"
				StateCount
			};
			#undef REGISTER_ENUM

			// Getters
			inline bool IsActive() const { return isActive; }
			inline eCameraState GetState() const { return state; }

			// Setterss
			inline void SetIsActive(const bool in_isActive) { isActive = in_isActive; }
			inline void SetState(const eCameraState in_state) { state = in_state; }

		private:
			bool isActive = false;

			eCameraState state = eCameraState::Follow;
		};

	} // namespace Components
} // namespace Mega