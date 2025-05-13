#pragma once

#include "Engine/Engine.h"

class CharacterController : public Mega::Entity
{
public:
	using Vec2 = Mega::Vec2;
	using Vec3 = Mega::Vec3;
	using tScalar = Mega::PhysicsSystem::tScalar;

	// A trick to keep the movement enums and their
	// string counterparts consistent
	#define REGISTER_ENUM(state) state,
	enum eMovementState : int32_t
	{
		#include "CharacterStates.h"
		StateCount
	};
	#undef REGISTER_ENUM

	enum class eMovementInput : int32_t
	{
		None = 0,
		Right,
		Left,
		Forward,
		Back,
		Up,
		Down,
		Shift,
	};

	// ---------------- Member Functions ----------------
	CharacterController() = default;

	void OnInitialize() override;
	void OnUpdate(const Mega::tTimestep in_dt) override;
	void OnUpdatePost(const Mega::tTimestep in_dt) override;
	void OnCollision(const Mega::Entity* in_pEntity, const Mega::CollisionData& in_collisionData) override;
	void OnDestroy() override {};

protected:
	// To control using keyboard input
	void ControlWithInput(const Mega::Input& in_input);

	// Input Getters/Setters
	inline void ClearInputFields() { m_inputs = 0b0; }
	inline void SetInputField(const eMovementInput in_input, bool in_value) { m_inputs ^= (-in_value ^ (uint8_t)in_input) & (1 << (uint8_t)in_input); }
	inline bool IsPressing(const eMovementInput in_input) const { return m_inputs & (1 << (uint8_t)in_input); }
	inline bool IsInputting() const { return m_inputs != 0; }
	// inline bool JustPressed(const eMovementInput in_input);
	// inline bool JustReleased(const eMovementInput in_input);

	// Movement
	void SetMovementDirection(const Mega::Vec3& in_direction) { m_movementDirection = in_direction; }
	void SetFacingDirection(const Mega::Vec3& in_direction) { m_facingDirection = in_direction; }
	const Vec3& GetFacingDirection() const { return m_facingDirection; }

	void Move(const Mega::Vec3& in_direction, const tScalar in_speed);
	void Run(const Mega::Vec3& in_direction);
	void Dash(const Mega::Vec3& in_direction);
	void Float(const Mega::Vec3& in_direction);
	void Jump(Mega::Vec3 in_direction = Vec3(0, 1, 0));
	void WallClimb(const Mega::Vec2& in_direction);

	inline bool IsGrounded() const { return m_groundRayTest || m_groundCollisionTest; }
	inline bool IsOnWall()   const { return m_wallRayTest;   }

	// TODO: Make more specific "CanMove" functions
	inline bool CanMove() const { return (IsGrounded() && !IsState(eMovementState::Dashing)); } // TODO: fix to include floating

	// State
	inline eMovementState MovementState() const { return (eMovementState)m_stateMachine.GetState(); }
	inline eMovementState LastMovementState() const { return (eMovementState)m_stateMachine.GetLastState(); }
	inline const char* MovementStateName(eMovementState in_state) const { return s_stateNames[in_state]; }
	inline Mega::tTimestep MovementStateTimer(eMovementState in_state) const { return m_stateMachine.GetStateTimer(in_state); }

	// Timers // TODO: Be able to set default time buffer for each state
	inline Mega::tTime TimeSinceLastMovementState() const { return MovementStateTimer(LastMovementState()); }
	inline bool IsState(eMovementState in_state) const { return m_stateMachine.IsState(in_state); }

	inline Mega::Vec3 GetLinearVelocity() const { return m_pPhysicsBody->GetLinearVelocity(); }

private:
	void InterpolateRotation();
	inline void SetState(eMovementState in_state) { m_stateMachine.SetState(in_state); }

	// ---------------- Physics Helpers ---------------
	inline void ApplyCentralForce(const Mega::Vec3& in_force) { m_pPhysicsBody->pPhysicsBody->applyCentralForce({ in_force.x, in_force.y, in_force.z }); }
	inline void ApplyCentralImpulse(const Mega::Vec3& in_force) { m_pPhysicsBody->pPhysicsBody->applyCentralImpulse({ in_force.x, in_force.y, in_force.z }); }
	inline void SetLinearVelocity(const Mega::Vec3& in_velocity) { m_pPhysicsBody->SetLinearVelocity(in_velocity); }
	// ------------------------------------------------

	// ---------------- State Managers ----------------
	void OnEntryJumping();
	void OnEntryWallClimbing();
	void OnEntryFloating();

	void OnUpdateIdle();
	void OnUpdateRunning();
	void OnUpdateJumping();
	void OnUpdateFalling();
	void OnUpdateDashing();
	void OnUpdateWallClimbing();
	void OnUpdateWallJumping();

	void OnExitWallClimbing();
	// ------------------------------------------------

	// ---------------- Member Variables ----------------
	Mega::StateMachine<CharacterController> m_stateMachine;

	tScalar m_gravity = -9.8f;
	
	// Input Controls
	uint8_t m_inputs = 0b0; // Input bit field

	// Floating
	tScalar m_floatSpeed = 1.0f;

	// Wall Climbing
	Vec3 m_wallCollisionNormal = { 0, 0, 0};
	tScalar m_wallStickForce = 40.0f;
	tScalar m_wallClimbForce = 0.5f; // TODO: speed or force?

	Mega::tTimestep m_dt = 0.0f;
	tScalar m_acceleration = 0.1f;
	tScalar m_rotationSpeed = 1.5f;
	tScalar m_runSpeed = 2.0f;
	tScalar m_sprintSpeed = 2.0f;
	tScalar m_dashSpeed = 10.0f;
	tScalar m_rollSpeed = 3.0f;
	tScalar m_jumpHeight = 50.0f;

	Mega::tTimestep m_dashTime = 120;
	Mega::tTimestep m_dashResetTime = 1000;
	Mega::tTimestep m_jumpBuffer = 150;
	Mega::tTimestep m_jumpReloadTime = 400;
	Mega::tTimestep m_wallResetTime = 300;

	Vec3 m_movementDirection = { 0, 0, 1 }; // Direction force is being applied
	Vec3 m_facingDirection   = { 0, 0, 1 }; // Direction model is facing (or should be)
	Vec3 m_inputDirection3D  = { 0, 0, 0 }; // Direction player has inputted

	// ------------- State Booleans -------------
	bool m_wallRayTest = false;
	bool m_wallRayTestRight = false;
	bool m_wallRayTestLeft = false;
	bool m_groundRayTest = false;
	bool m_groundCollisionTest = false;
	// ------------------------------------------

	Mega::Component::RigidBody* m_pPhysicsBody = nullptr;
	Mega::Component::CollisionCapsule* m_pCollisionShape = nullptr;

	static const char* s_stateNames[];
};