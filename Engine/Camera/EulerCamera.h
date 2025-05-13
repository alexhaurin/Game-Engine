 #pragma once

#include "Engine/Core/Math/Math.h"

#define CAMERA_POSITION 0.0f, 0.0f, 5.0f
#define CAMERA_FORWARD  0.0f, 0.0f, -1.0f
#define CAMERA_UP       0.0f, 1.0f, 0.0f
#define CAMERA_UP_X 0.0f
#define CAMERA_UP_Y 1.0f
#define CAMERA_UP_Z 0.0f
#define CAMERA_SCALAR float

namespace Mega
{
	class EulerCamera {
	public:
		using tScalar = CAMERA_SCALAR;

		// =========== Perspective Stuff ============ //
		virtual Mat4x4 GetProjectionMatrix(tScalar in_aspectRation, tScalar in_near, tScalar in_far) const;
		static Mat4x4 GetConstLookAtMatrix();
		virtual Mat4x4 GetLookAtMatrix() const;

		inline void SetFOV(const tScalar in_fovDegrees) { m_FOVDegrees = in_fovDegrees; }

		void LookAtPoint(const Vec3& in_point);

		// =========== Getters ================ //
		Vec3 GetDirection() const;
		Vec3 GetUp() const;
		Vec3 GetForward() const { return m_forward; }
		Vec3 GetPosition() const { return m_position; }

		inline Vec3 GetRotation() const { return { GetPitch(), GetYaw(), GetRoll() }; };
		inline float GetYaw() const { return m_yaw; }
		inline float GetPitch() const { return m_pitch; }
		inline float GetRoll() const { return m_roll; }

		// ----------------- Setters ----------------- //
		inline void SetPosition(const Vec3& in_pos) { m_position = in_pos; }
		inline void SetYaw(float in_yaw) { m_yaw = in_yaw; }
		inline void SetPitch(float in_pitch) { m_pitch = in_pitch; }
		//void SetRoll(float in_roll) { m_roll = in_roll; }

		// ----------------- Movement ----------------- //
		inline void Move(const Vec3& in_movement) { m_position += in_movement; }
		void Strafe(const float in_movement);
		void Forward(const float in_movement);
		void Up(const float in_movement);

		inline void RotatePitch(const float in_rad) { m_pitch += in_rad; }
		inline void RotateYaw(const float in_rad) { m_yaw += in_rad; }
		// inline void RotateRoll(float in_rad)  { m_roll += in_rad; }

	private:
		Vec3 m_position = Vec3(CAMERA_POSITION);
		Vec3 m_forward = Vec3(CAMERA_FORWARD);
		Vec3 m_up = Vec3(CAMERA_UP);

		tScalar m_pitch = 0.0f;
		tScalar m_yaw = 0.0f;
		tScalar m_roll = 0.0f;

		tScalar m_FOVDegrees = 45.0f;
	};
} // namespace Mega