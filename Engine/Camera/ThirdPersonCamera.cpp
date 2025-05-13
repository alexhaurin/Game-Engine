//#include "ThirdPersonCamera.h"
//
//bool g_shake = false;
//ThirdPersonCamera::ThirdPersonCamera(Player* in_pPlayer, Mega::Scene* in_pScene)
//{
//	MEGA_ASSERT(in_pPlayer, "Player is nullptr");
//	MEGA_ASSERT(in_pScene, "Scene is nullptr");
//
//	m_pPlayer = in_pPlayer;
//	m_pScene = in_pScene;
//}
//
//// We create a radius that represents the shake magnitude then pick a random point within the radius
//// and move the camera there while we decrease the cmaera magnitude
//void ThirdPersonCamera::Shake(const Mega::tTimestep in_dt)
//{
//	if (currentShakeTime <= 0) { return; }
//
//	tempPitch = GetPitch();
//	tempYaw = GetYaw();
//
//	float x = Mega::RandomFloat(-currentShakeRadius, currentShakeRadius);
//	float y = Mega::RandomFloat(-currentShakeRadius, currentShakeRadius);
//	float pitch = Mega::RandomFloat(-currentShakeRadius / 50.0f, currentShakeRadius / 50.0f);
//	float yaw = Mega::RandomFloat(-currentShakeRadius / 50.0f, currentShakeRadius / 50.0f);
//	currentShakeRadius = currentShakeRadius - (currentShakeRadius * Mega::SquaredAcceleration(currentShakeTime/shakeTime));
//	currentShakeTime -= in_dt / 1000.0f;
//
//	SetPosition(positionOffset + m_pPlayer->GetPosition() + Mega::Vec3(x, 0, y));
//}
//
//void ThirdPersonCamera::Update(const Mega::tTimestep in_dt)
//{
//	ImGui::DragFloat3("Position", &positionOffset.x, 0.1);
//	ImGui::DragFloat3("Rotation", &lookatOffset.x, 0.01);
//	//ImGui::DragFloat("Shake Time: ", &shakeTime);
//	//ImGui::DragFloat("Shake Radius", &shakeRadius);
//	//shake = ImGui::Button("Camera Shake");
//
//	if (g_shake)
//	{
//		currentShakeRadius = shakeRadius;
//		currentShakeTime = shakeTime;
//	}
//
//	SetPosition(positionOffset + m_pPlayer->GetPosition());
//	LookAtPoint(lookatOffset + m_pPlayer->GetPosition());
//	Shake(in_dt);
//}