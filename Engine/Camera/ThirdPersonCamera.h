//#pragma once
//
//#include "Engine/Engine.h"
//#include "Game/Characters/Player/Player.h"
//
//class ThirdPersonCamera : public Mega::EulerCamera
//{
//public:
//	ThirdPersonCamera(Player* in_pPlayer, Mega::Scene* in_pScene);
//
//	// We create a radius that represents the shake magnitude then pick a random point within the radius
//	// and move the camera there while we decrease the cmaera magnitude
//	void Shake(const Mega::tTimestep in_dt);
//	void Update(const Mega::tTimestep in_dt);
//
//private:
//	Player* m_pPlayer;
//	Mega::Scene* m_pScene;
//
//	// Offsets for top view
//	// Mega::Vec3 positionOffset = { 0.0, 60.0, -140.0 };
//	// Mega::Vec3 lookatOffset = { 0.0, 30, -10 };
//
//	// Offsets for close side view
//	Mega::Vec3 positionOffset = { -58.7, 68.8, -28.2 };
//	Mega::Vec3 lookatOffset = { -6.82, -2.81, 23.64 };
//
//	tScalar tempPitch = 0.0f;
//	tScalar tempYaw = 0.0f;
//
//	tScalar currentShakeRadius = 0.0f;
//	tScalar currentShakeTime = 0.0f;
//	tScalar shakeRadius = 0.0f;
//	tScalar shakeTime = 0.0f;
//};