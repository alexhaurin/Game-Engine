#pragma once

#include "Engine/ECS/System.h"

// Forward Declarations
namespace Mega
{
	class Scene;
	class EulerCamera;
}

namespace Mega
{
	class CameraSystem : public System
	{
	public:
		friend Engine;

		using Vec3 = Mega::Vec3;
		using tScalar = Mega::tScalarPrecision;

		eMegaResult OnInitialize() override;
		eMegaResult OnDestroy() override;
		eMegaResult OnUpdate(const tTimestep in_dt, Scene* in_pScene) override;

		inline const EulerCamera* GetActiveCamera() const { return m_pActiveCamera; }

	private:
		EulerCamera* m_pActiveCamera = nullptr;

		// Camera state
		Mega::StateMachine<CameraSystem> m_stateMachine;
		static const char* s_stateNames[];

		// Used for lerping/slerping
		Vec3 m_cameraTargetPosition = { 0, 0, 0 };
	};
}