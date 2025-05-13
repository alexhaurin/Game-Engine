#include "CharacterController.h"

#include <minmax.h>
#include "Game/Objects/Arena.h"
#include "Engine/Objects/Wall.h"

// Register Character State enum
#define REGISTER_ENUM(state) #state,
const char* CharacterController::s_stateNames[] =
{
	#include "CharacterStates.h",
	"Invalid"
};
#undef REGISTER_ENUM

// ------------------------ CharacterController Functions ------------------------
void CharacterController::OnInitialize()
{
	AddComponent<Mega::Component::CollisionCapsule>(0.5, 1.8); // Meters
	AddComponent<Mega::Component::RigidBody>(Mega::PhysicsSystem::eRigidBodyType::Dynamic, 10.0f, 0.5f, 0.0f);

	m_pPhysicsBody = &GetComponent<Mega::Component::RigidBody>();
	m_pPhysicsBody->SyncRotation(false);
	m_pPhysicsBody->SetGravity(m_gravity);
	m_pPhysicsBody->localOffset = { 0, 1, 0 };
	m_pCollisionShape = &GetComponent<Mega::Component::CollisionCapsule>(); // TODO: more general get collision shape component function?

	m_stateMachine = Mega::StateMachine<CharacterController>(this, eMovementState::StateCount);
	m_stateMachine.AddState(eMovementState::Idle, &CharacterController::OnUpdateIdle);
	m_stateMachine.AddState(eMovementState::Running, &CharacterController::OnUpdateRunning);
	m_stateMachine.AddState(eMovementState::Dashing, &CharacterController::OnUpdateDashing);
	m_stateMachine.AddState(eMovementState::Jumping, &CharacterController::OnUpdateJumping, &CharacterController::OnEntryJumping);
	m_stateMachine.AddState(eMovementState::Falling, &CharacterController::OnUpdateFalling);
	m_stateMachine.AddState(eMovementState::WallClimbing, &CharacterController::OnUpdateWallClimbing, &CharacterController::OnEntryWallClimbing, &CharacterController::OnExitWallClimbing);

	SetState(eMovementState::Idle);
}

void CharacterController::ControlWithInput(const Mega::Input& in_input)
{
	// ------------------ Update Inputs -----------------------
	// Rotate around target position based on mouse movement
	const Vec2 mouseMovement = Vec2(in_input.mousePosX, in_input.mousePosY);

	// ------- Mouse Input -------
	// Set the facing direction usnig the mouse
	const float angle = mouseMovement.x / 200.0f;
	const Vec2 forwardVector = Vec2(cos(angle), sin(angle));
	const Vec2 perpendicularVector = Vec2(-1.0f * forwardVector.y, forwardVector.x);
	SetFacingDirection({ -forwardVector.x, 0, -forwardVector.y });

	// ------- Keyboard Input -------
	if (in_input.keyW)      { SetInputField(eMovementInput::Forward, true); }
	if (in_input.keyS)      { SetInputField(eMovementInput::Back,    true); }
	if (in_input.keyD)      { SetInputField(eMovementInput::Right,   true); }
	if (in_input.keyA)      { SetInputField(eMovementInput::Left,    true); }
	if (in_input.keySpace)  { SetInputField(eMovementInput::Up,      true); }
	if (in_input.keyLShift) { SetInputField(eMovementInput::Shift,   true); }

	// ------------------ Move Based On Keyboard/Mouse Input ------------------
	// Gather a direction vector
	Vec2 inputDirection = Vec2(0);
	if (IsPressing(eMovementInput::Back))    { inputDirection += forwardVector; } // Already normalized
	if (IsPressing(eMovementInput::Forward)) { inputDirection -= forwardVector; }
	if (IsPressing(eMovementInput::Left))    { inputDirection += perpendicularVector; }
	if (IsPressing(eMovementInput::Right))   { inputDirection -= perpendicularVector; }
	if (length(inputDirection) > 0) { inputDirection = normalize(inputDirection); }
	m_inputDirection3D = Vec3(inputDirection.x, 0, inputDirection.y);

	if (length(inputDirection) > 0)
	{
		Run(m_inputDirection3D);
		SetFacingDirection(m_inputDirection3D);
	}
	// Move
	if (IsPressing(eMovementInput::Up))
	{
		Jump();
	}
}

void CharacterController::OnUpdate(const Mega::tTimestep in_dt)
{
	// Frame Setup
	m_dt = in_dt;

	// ImGui
	ImGui::Text(MovementStateName(MovementState()));
	ImGui::Text("Last State:");
	ImGui::Text(MovementStateName(LastMovementState()));
	auto timer = MovementStateTimer(MovementState());
	ImGui::DragFloat("Current State Timer", &timer);
	Vec3 pos = GetPosition(); ImGui::DragFloat3("Player Position", &pos.x);

	//ImGui::DragFloat("Jump Buffer", &m_jumpBuffer, 0.01, 0.0);
	//ImGui::DragFloat("Jump Height", &m_jumpHeight, 1.0, 0.0);
	ImGui::Checkbox("On Ground", &m_groundRayTest);
	ImGui::Checkbox("On Wall", &m_wallRayTest);

	// Update state machine
	m_stateMachine.Update(in_dt);

	// Cleanup before physics
	m_groundCollisionTest = false;
}

float wallBuffer = 0.8; // How far the ray comes out of the player
float wallStart = 0.5;
void CharacterController::OnUpdatePost(const Mega::tTimestep in_dt)
{
	// Perform RayTests after physics (and other systems) are done to be valid for the next frame
	const tScalar& capsuleRadius = m_pCollisionShape->radius;

	const Vec3& pos = GetPosition();
	const Vec3& facing = GetFacingDirection();

	const tScalar rayBuffer = 0.5; // How far the ray comes out of the player
	const tScalar rayStart = 0.2;
	m_groundRayTest = Mega::Engine::PerformRayTestCollision(pos + Vec3(0, rayStart, 0), pos + Vec3(0, -rayBuffer, 0));
	m_wallRayTest = Mega::Engine::PerformRayTestCollision(pos + Vec3(0, wallStart, 0), pos + (facing * (capsuleRadius + wallBuffer)));

	if (IsState(eMovementState::Jumping) && MovementStateTimer(eMovementState::Jumping) < m_jumpBuffer) { m_groundRayTest = false; }

	// Deaccelerate
	const auto& vel = GetLinearVelocity();
	SetLinearVelocity({ 0, vel.y, 0 });

	// Frame Cleanup
	ClearInputFields();
}

void CharacterController::OnCollision(const Mega::Entity* in_pEntity, const Mega::CollisionData& in_collisionData)
{
	// Bug: falling off cliff can still jump once
	if (TypeOf(in_pEntity) == TypeOf<Mega::Wall>() || TypeOf(in_pEntity) == TypeOf<Arena>())
	{
		const btPersistentManifold* pManifold = in_collisionData.pManifold;
		for (int i = 0; i < pManifold->getNumContacts(); i++)
		{
			const btManifoldPoint& contact = pManifold->getContactPoint(i);

			if (contact.m_normalWorldOnB.getY() == 1)
			{
				if (IsState(eMovementState::Jumping) && MovementStateTimer(eMovementState::Jumping) < m_jumpBuffer) { return; }

				m_groundCollisionTest = true;
			}
		}
	}
}

// ============ State Managers ============== //
void CharacterController::OnEntryFloating()
{
	// Set the float speed using the last state
	m_floatSpeed = 20; // glm::length(Vec2(GetLinearVelocity().x, GetLinearVelocity().z)) * 50;
	if (LastMovementState() == eMovementState::Idle) { m_floatSpeed = 0; }

	// TODO: LastState() vs IsState() vs LastMovementState()
}
void CharacterController::OnEntryJumping()
{
	
}
void CharacterController::OnEntryWallClimbing()
{
	m_pPhysicsBody->SetGravity(0.0f);

	SetLinearVelocity({ 0, 0, 0 });
}

void CharacterController::OnExitWallClimbing()
{
	m_pPhysicsBody->SetGravity(m_gravity);
}
void CharacterController::OnUpdateWallJumping()
{
	if (IsGrounded()) { SetState(eMovementState::Idle); }

	Float(m_movementDirection);
}
void CharacterController::OnUpdateJumping()
{	
	if (IsGrounded() && MovementStateTimer(eMovementState::Jumping) > m_jumpBuffer) { SetState(eMovementState::Idle); }

	if (GetLinearVelocity().y <= 0) { SetState(eMovementState::Falling); }
	Float(m_inputDirection3D);

	InterpolateRotation();
}
void CharacterController::OnUpdateFalling()
{
	if (IsGrounded()) { SetState(eMovementState::Idle); }

	Float(m_inputDirection3D);

	InterpolateRotation();
}
void CharacterController::OnUpdateDashing()
{
	if (MovementStateTimer(eMovementState::Dashing) >= m_dashTime) { SetState(LastMovementState()); }

	Move(m_movementDirection, m_dashSpeed);
}
void CharacterController::OnUpdateIdle()
{
	// Deaccelerate
	if (IsGrounded())
	{
		SetLinearVelocity({ 0, m_gravity /*GetLinearVelocity().y*/, 0});
	}

	InterpolateRotation();
}
void CharacterController::OnUpdateRunning()
{
	if (!IsGrounded()) { SetState(eMovementState::Falling); }
	else { SetState(eMovementState::Idle); }

	InterpolateRotation();
}
void CharacterController::OnUpdateWallClimbing()
{
	//if (IsGrounded() || !IsOnWall())
	//{
	//	if (IsGrounded()) { SetState(eMovementState::Idle); }
	//	else { SetState(eMovementState::WallJumping); }
	//}
	//
	//// Set Facing
	//SetFacingDirection(-m_wallCollisionNormal); // Yaw
	//
	//// This and turning off gravity let player stick to the wall
	//ApplyCentralForce(m_wallCollisionNormal * -m_wallStickForce);
}

// --------------------- Movement ------------------ //
// Assume input direction is normalized
void CharacterController::Move(const Mega::Vec3& in_direction, tScalar in_speed)
{
	// TODO change these to asserts of something more release friendly (or just remove)
	if (length(in_direction) == 0) { return; }
	if (in_speed == 0) { return; }

	SetMovementDirection(in_direction);

	// Acceleration is the minimum of our currentSpeed / our max speed or just 1.0
	// (which makes it a constant time for all speeds aka walk and run have the same speed up time)
	Mega::Vec3 linearForce = { GetLinearVelocity().x, 0.0f, GetLinearVelocity().z };
	tScalar speedFactor = glm::length(linearForce) / in_speed;
	tScalar acceleration = min(1.0f, (float)(speedFactor + m_acceleration));
	// Final movement:
	auto movement = m_movementDirection * in_speed * acceleration * m_dt;
	SetLinearVelocity({ movement.x, GetLinearVelocity().y, movement.z });
}
void CharacterController::Run(const Mega::Vec3& in_direction)
{
	if (!CanMove()) { return; }
	SetState(eMovementState::Running);

	Move(in_direction, m_runSpeed);
}
void CharacterController::Dash(const Mega::Vec3& in_direction)
{
	if (IsState(eMovementState::Dashing)) { return; }
	if (MovementStateTimer(eMovementState::Dashing) < m_dashResetTime) { return; }

	SetState(eMovementState::Dashing);

	Move(in_direction, m_dashSpeed);
}
void CharacterController::Float(const Mega::Vec3& in_direction)
{
	if (IsGrounded()) { return; }
	if (LastMovementState() == eMovementState::Idle) { return; }

	Move(in_direction, m_floatSpeed * 10);
}
// Assume input direction is normalized
void CharacterController::Jump(Mega::Vec3 in_direction)
{
 	if (!IsGrounded() && !IsOnWall()) { return; }
	if (MovementStateTimer(eMovementState::Falling) < m_jumpReloadTime) { return; }
	if (MovementStateTimer(eMovementState::Jumping) < m_jumpReloadTime) { return; }
	
	// Apply force
	Vec3 jumpForce = { 0, 0, 0 };

	// TODO
	jumpForce = { 0, m_jumpHeight, 0 };

	ApplyCentralImpulse({ jumpForce });
	SetState(eMovementState::Jumping);
}
void CharacterController::WallClimb(const Mega::Vec2& in_direction)
{
	if (!IsState(eMovementState::WallClimbing)) { return; }
	if (length(in_direction) == 0) { return; }

	// Force will be perpendicular to wall collision normal, so we need to find
	// the vector that runs along the side of the wall
	Vec3 upVector = { 0, 1, 0 };
	Vec3 sideVector = glm::cross(upVector, m_wallCollisionNormal); // upVector and collision are both normal so sideVector should be too

	//std::cout << sideVector.x << ", " << sideVector.y << ", " << sideVector.z << std::endl;

	// Apply horizontal and then vertical forces
	Vec2 directionNormal    = normalize(in_direction);
	Vec3 horizontalMovement = -1.0f * sideVector * directionNormal.x;
	Vec3 verticalMovement   = upVector * directionNormal.y;

	Vec3 temp1 = (horizontalMovement + verticalMovement);
	SetLinearVelocity((horizontalMovement + verticalMovement) * m_wallClimbForce * m_dt);

	// Re-apply wall stick
	ApplyCentralForce(m_wallCollisionNormal * -m_wallStickForce);
}

// ------------------- Private Helpers ------------------------- //
void CharacterController::InterpolateRotation()
{
	// We interpolate the rotation from current direction to desired direction to make the character rotate smoothly
	Mega::Vec3 desiredDir = GetFacingDirection();
	tScalar desiredYaw = std::atan2(desiredDir.x, desiredDir.z) + PI_F; // Our radians are -PI to PI but we convert to 0 to 2PI for the calculation
	tScalar currentYaw = GetRotation().y + PI_F; // GetRotation() is already in Euler angles so no calculation needed
	tScalar yawDifference = desiredYaw - currentYaw;

	if (yawDifference == 0) { return; }

	// Unshamefully stolen from stack overflow because this bug was mad weird
	if (abs(yawDifference) > PI_F)
	{
		// A little hack to solve a rotation issue: when the player was going from say -3.0 radians to 3.0 radians
		// and crossing the -PI, PI boundry on the unit circle instead of crossing the boundry it go the long way around
		// from -3.0 to -2.0 to -1.0 ... all the way to 3.0 instead of just crossing from -3.0 to 3.0
		if (desiredYaw > currentYaw) { currentYaw += PI_F * 2; }
		else { desiredYaw += PI_F * 2; }
	}

	// Interpolate value
	tScalar rot = currentYaw + ((desiredYaw - currentYaw) * (m_rotationSpeed / 100.0f) * m_dt); // / divide rotSpeed cause otherwise it would be crazy small
	rot = fmod(rot, PI_F * 2); // Wrap it to 0 - 2PI
	currentYaw = rot - PI_F; // convert back to -PI to PI radians

	// Rotate
	SetRotation({ 0, currentYaw, 0 });
}