#pragma once

#include "Player.h"
#include "Engine/Engine.h"
#include "Game/Characters/CharacterController.h"

bool g_lockPlayer = false;
void Player::OnInitialize()
{
	CharacterController::OnInitialize();

	m_pCameraTarget = &AddComponent<Mega::Component::CameraTarget>();
	m_pCameraTarget->SetIsActive(true);
	m_pCameraTarget->SetState(Mega::Component::CameraTarget::eCameraState::Follow); // Man I fucking love namespaces but fr I gotta fix this

	// Player
	Mega::AnimatedMesh mesh = Mega::Engine::LoadAnimatedMesh("Assets/Animations/Player/mesh.ozz");
	Mega::AnimatedSkeleton skeleton = Mega::Engine::LoadAnimatedSkeleton("Assets/Animations/Player/skeleton.ozz");

	m_pSoundPlayer = &AddComponent<Mega::Component::SoundPlayer>(); // "Walking", Mega::Engine::LoadSound("Assets/Sounds/test.wav"));
	m_pSoundPlayer->AddSoundEffect("Walking", Mega::Engine::LoadSound("Assets/Sounds/footstepGrass.wav"));
	m_pSoundPlayer->SetGain(0.3f);

	m_pAnimation = &AddComponent<Mega::Component::AnimatedModel>(mesh, skeleton);
	m_pAnimation->AddAnimation("Idle", Mega::Engine::LoadAnimation("Assets/Animations/Player/Idle.ozz"));
	m_pAnimation->AddAnimation("Running", Mega::Engine::LoadAnimation("Assets/Animations/Player/Run.ozz"));
	m_pAnimation->AddAnimation("Jumping", Mega::Engine::LoadAnimation("Assets/Animations/Player/JumpStart.ozz"), false);
	m_pAnimation->AddAnimation("Falling", Mega::Engine::LoadAnimation("Assets/Animations/Player/JumpLoop.ozz"));

	m_pWindMotor = &AddComponent<Mega::Component::WindMotor>();
};

void Player::OnUpdate(const Mega::tTimestep in_dt)
{
	ImGui::Checkbox("Lock Player", &g_lockPlayer);
	if (!g_lockPlayer) { CharacterController::ControlWithInput(Mega::Engine::GetInput()); }
	CharacterController::OnUpdate(in_dt);
}

void Player::OnUpdatePost(const Mega::tTimestep in_dt)
{
	float fps = (1.0f / in_dt) * 1000;
	ImGui::SliderFloat("FPS", &fps, 0, 300);
	PlayCorrectAnimation();
	CharacterController::OnUpdatePost(in_dt);
}

float blend = 0.0;
float blendSpeed = 100;
float pitch = 170.0f; // TODO: make sound length in milli
void Player::PlayCorrectAnimation()
{
	const auto StateToAnim = [](const eMovementState state) -> const char*
	{
		if (state == eMovementState::Idle) { return "Idle"; }
		else if (state == eMovementState::Jumping) { return "Jumping"; }
		else if (state == eMovementState::Falling) { return "Falling"; }
		else { return "Running"; }
	};

	const char* lastAnim = StateToAnim(LastMovementState());
	const char* currentAnim = StateToAnim(MovementState());
	const Mega::tTimestep timer = MovementStateTimer(MovementState());

	blend = std::min<float>(timer / blendSpeed, 1.0f);
	m_pAnimation->Blend(lastAnim, currentAnim, blend, &GetComponent<Mega::Component::Transform>());

	// ------------ Leg and Feet Planting ------------- //
	auto facing = GetFacingDirection();
	glm::vec2 perp1 = { GetFacingDirection().z, -GetFacingDirection().x };
	glm::vec2 perp2 = { -GetFacingDirection().z, GetFacingDirection().x };
	glm::vec3 leftFootPosition = GetPosition() + Vec3(perp1.x / 4.0, 0, perp1.y / 4.0); // TODO: dont hardcode joint positions bozo
	glm::vec3 rightFootPosition = GetPosition() + Vec3(perp2.x / 4.0, 0, perp2.y / 4.0);

	// Foot Planting
	auto rayTestPositionLeft = Mega::Engine::PerformRayTestPosition({ leftFootPosition.x, GetPosition().y + 1, leftFootPosition.z }, { leftFootPosition.x, GetPosition().y - 10, leftFootPosition.z });
	auto rayTestPositionRight = Mega::Engine::PerformRayTestPosition({ rightFootPosition.x, GetPosition().y + 1, rightFootPosition.z }, { rightFootPosition.x, GetPosition().y - 10, rightFootPosition.z });
	rayTestPositionLeft.y -= 0.05f;
	rayTestPositionRight.y -= 0.05f;

	auto rayTestNormalLeft = Mega::Engine::PerformRayTestNormal({ leftFootPosition.x, GetPosition().y + 1, leftFootPosition.z }, { leftFootPosition.x, GetPosition().y - 10, leftFootPosition.z });
	auto rayTestNormalRight = Mega::Engine::PerformRayTestNormal({ rightFootPosition.x, GetPosition().y + 1, rightFootPosition.z }, { rightFootPosition.x, GetPosition().y - 10, rightFootPosition.z });

	ImGui::DragFloat("Sound Buffer", &pitch, 0.001, 0.0, 1000);
	m_pSoundPlayer->SetBuffer(pitch);
	if (MovementState() == eMovementState::Running)
	{
		m_pSoundPlayer->Play("Walking");
		Mega::Engine::SetWindSimulationCenter(GetPosition() * Vec3(1, 0, 1));
		m_pWindMotor->SetPosition(Vec3(25, 0, 25));
		m_pWindMotor->Directional(GetFacingDirection(), 20.0f);
	}
	if (MovementState() == eMovementState::Idle)
	{
		m_pAnimation->InverseKinematics("thigh_l", "calf_l", "foot_l", rayTestPositionLeft, facing, &GetComponent<Mega::Component::Transform>());
		m_pAnimation->InverseKinematics("thigh_r", "calf_r", "foot_r", rayTestPositionRight, facing, &GetComponent<Mega::Component::Transform>());
		m_pAnimation->PlantFoot("foot_l", rayTestNormalLeft * Vec3(100, -100, -100), facing, &GetComponent<Mega::Component::Transform>());
		m_pAnimation->PlantFoot("foot_r", rayTestNormalRight * Vec3(100, -100, -100), facing, &GetComponent<Mega::Component::Transform>());
	}
}

void Player::OnCollision(const Mega::Entity* in_pEntity, const Mega::CollisionData& in_collisionData)
{
	CharacterController::OnCollision(in_pEntity, in_collisionData);
}

void Player::OnDestroy()
{
	CharacterController::OnDestroy();
}