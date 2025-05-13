#pragma once

#include "Engine/Engine.h"

#include "Game/Characters/Player/Spear.h"
#include "Game/Characters/CharacterController.h"

class Player : public CharacterController
{
public:
	using Vec3 = Mega::Vec3;
	using Vec2 = Mega::Vec2;

	void OnInitialize() override;
	void OnUpdate(const Mega::tTimestep in_dt) override;
	void OnUpdatePost(const Mega::tTimestep in_dt) override;
	void OnCollision(const Mega::Entity* in_pEntity, const Mega::CollisionData& in_collisionData) override;
	void OnDestroy() override;

private:
	void PlayCorrectAnimation();

	Spear* m_pSpear = nullptr;
	Spear* m_pIKTargetModel = nullptr;
	Spear* m_pIKTargetModel2 = nullptr;

	Mega::Component::SoundPlayer* m_pSoundPlayer = nullptr;
	Mega::Component::AnimatedModel* m_pAnimation = nullptr;
	Mega::Component::WindMotor* m_pWindMotor = nullptr;
	Mega::Component::CameraTarget* m_pCameraTarget = nullptr;
};