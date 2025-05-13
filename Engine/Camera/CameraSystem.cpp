#include "CameraSystem.h"

#include "Engine/Engine.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Input.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/ECS/Components.h"

#include "Engine/Camera/EulerCamera.h"
#include "Engine/Camera/CameraComponents.h"
#include "Engine/Camera/ThirdPersonCamera.h"
#include "Engine/Camera/OrthographicCamera.h"

Mega::Vec3 g_lookAtOffset = Mega::Vec3(-0.3, 0.6, -3.6);
Mega::Vec3 g_posOffset = Mega::Vec3(-0.7, 0.3, -8.0);
float g_lockDistanceX = 12.0f;
float g_lockDistanceY = 5.0f;
bool g_lockCamera = false;

Mega::Vec3 g_camLookAt = { 0, 12, 1.5 };
Mega::Vec3 g_camPos    = { -12, 23.5, 31 };

#define REGISTER_ENUM(state) #state,
const char* Mega::CameraSystem::s_stateNames[] =
{
	#include "CameraStates.h",
	"Invalid"
};
#undef REGISTER_ENUM

using namespace Mega::Component;
namespace Mega
{
	eMegaResult CameraSystem::OnInitialize()
	{
		// Create main camera
		m_pActiveCamera = new EulerCamera;

		// Set up statemachine
		//m_stateMachine = Mega::StateMachine<CameraSystem>(this, eMovementState::StateCount);
		//m_stateMachine.AddState(eMovementState::Idle, &CharacterController::OnUpdateIdle);
		//SetState(eMovementState::Idle);

		return eMegaResult::SUCCESS;
	}

	eMegaResult CameraSystem::OnDestroy()
	{
		delete m_pActiveCamera;

		return eMegaResult::SUCCESS;
	}

	eMegaResult CameraSystem::OnUpdate(const tTimestep in_dt, Scene* in_pScene)
	{
		// Determine which camera to display
		Component::CameraTarget* cameraTarget = nullptr;
		Component::Transform* targetsTransform = nullptr;
		{
			auto view = in_pScene->GetRegistry().view<Component::Transform, Component::CameraTarget>();
			for (const auto& [entity, transform, target] : view.each())
			{
				if (!target.IsActive()) { continue; }

				targetsTransform = &transform;
				cameraTarget = &target;

				break;
			}
		}

		MEGA_ASSERT(cameraTarget, "No active camera target found");
		MEGA_ASSERT(targetsTransform, "No active camera target found");

		// Use camera target component to calculate camera position
		const Vec3& currentPos = m_pActiveCamera->GetPosition();
		const Vec3& playerPos = targetsTransform->GetPosition();
		// Rotation is in euler angles, so use yaw to get 2D facing vector
		const Vec2& facing = GetDirectionVector(targetsTransform->GetRotation().y);
		const Vec3& playerDir = Vec3(facing.x, 0.0f, facing.y);

		
		ImGui::Checkbox("Lock Camera", &g_lockCamera);
		if (g_lockCamera) { return eMegaResult::SUCCESS; }
		switch (cameraTarget->GetState()) // TODO: camera state should be member variable of system? "Manual" state shouldn't need a target
		{
			case CameraTarget::eCameraState::Manual:
			{
				ImGui::DragFloat3("Camera Position", &g_camPos.x, 0.01f);
				ImGui::DragFloat3("Camera Look At", &g_camLookAt.x, 0.01f);
				m_pActiveCamera->SetPosition(g_camPos);
				m_pActiveCamera->LookAtPoint(g_camLookAt);

				break;

			}
			case CameraTarget::eCameraState::Follow:
			{
				// ImGui
				//ImGui::DragFloat3("Position Offset", &g_posOffset.x, 0.01f);
				//ImGui::DragFloat("Lock Distance X", &g_lockDistanceX, 0.01f);
				//ImGui::DragFloat3("Look At Offset", &g_lookAtOffset.x, 0.01f);
				//ImGui::DragFloat("Lock Distance Y", &g_lockDistanceY, 0.01f);
				if (g_lockCamera) { break; }

				// Rotate around target position based on mouse movement
				const Vec2 mouseMovement = Vec2(Engine::GetInput().mousePosX, Engine::GetInput().mousePosY);
				
				// Circel around targets position, radius
				float angle = mouseMovement.x / 200.0f;
				const Vec2 forwardFacingVector = Vec2(cos(angle), sin(angle));
				const Vec2 perpendicularFacingVector = Vec2(-1.0f * forwardFacingVector.y, forwardFacingVector.x);

				// Set Position
				{
					Vec2 offset = perpendicularFacingVector * g_posOffset.x;
					offset += forwardFacingVector * g_posOffset.z;

					Vec3 circle = Vec3(forwardFacingVector.x * g_lockDistanceX, mouseMovement.y / 50.0f, forwardFacingVector.y * g_lockDistanceX);

					Vec3 newPos = Vec3(offset.x, g_posOffset.y, offset.y) + playerPos + circle;
					m_pActiveCamera->SetPosition(newPos);
				}

				// Look at target point
				{
					Vec2 offset = perpendicularFacingVector * g_lookAtOffset.x;
					offset += forwardFacingVector * g_lookAtOffset.z;

					m_pActiveCamera->LookAtPoint(playerPos + Vec3(offset.x, g_lookAtOffset.y, offset.y));
				}
				// Lerp position
				//Vec3 newPos = { 0, 0, 0 };
				//if (length(m_cameraTargetPosition - currentPos) > 0.1f)
				//{
				//	//std::cout << SquaredAcceleration(abs(tp.x - currentPos.x) / (tp.x - m_cameraStartLerpPosition.x)) << std::endl;
				//
				//	//np.x = (tp.x - m_cameraStartLerpPosition.x) * SquaredAcceleration(abs(tp.x - currentPos.x) / (tp.x - m_cameraStartLerpPosition.x));
				//	//np.y = (tp.y - m_cameraStartLerpPosition.y) * SquaredAcceleration(abs(tp.y - currentPos.y) / (tp.y - m_cameraStartLerpPosition.y));
				//	//np.z = (tp.z - m_cameraStartLerpPosition.z) * SquaredAcceleration(abs(tp.z - currentPos.z) / (tp.z - m_cameraStartLerpPosition.z));
				//
				//	newPos = currentPos + (m_cameraTargetPosition - currentPos) / 10.0f;
				//	m_pActiveCamera->SetPosition(newPos);
				//}

				break;
			}
			default:
				MEGA_ASSERT(false, "No code for the current camera state");
				break;
		}

		return eMegaResult::SUCCESS;
	}
} // namespace Mega