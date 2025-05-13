#include "EulerCamera.h"

glm::mat4x4 MakeRotationMatrixX(float in_thetaRad);
glm::mat4x4 MakeRotationMatrixY(float in_thetaRad);
glm::mat4x4 MakeRotationMatrixZ(float in_thetaRad);
glm::vec3 MultiplyMatrixVector(const glm::mat4x4& m, const glm::vec3& v);
glm::vec3 MakeVec3(float x, float y, float z);
glm::mat4x4 MakeMat4(float v);

// =========================== EulerCamera Class =========================== //
namespace Mega
{
	// =========== Perspective Stuff ============ //
	Mat4x4 EulerCamera::GetProjectionMatrix(tScalar in_aspectRation, tScalar in_near, tScalar in_far) const
	{
		return glm::perspective(glm::radians(m_FOVDegrees), in_aspectRation, in_near, in_far);
	}

	Mat4x4 EulerCamera::GetConstLookAtMatrix() // TOOD: fix
	{
		Vec3 eye = Vec3(CAMERA_POSITION);
		Vec3 target = Vec3(CAMERA_POSITION) + Vec3(CAMERA_FORWARD);
		Vec3 up = Vec3(CAMERA_UP);

		return glm::lookAt(eye, target, up);
	}
	Mat4x4 EulerCamera::GetLookAtMatrix() const
	{
		Vec3 eye = m_position;
		Vec3 target = m_position + GetDirection();
		Vec3 up = GetUp();

		return glm::lookAt(eye, target, up);
	}
	void EulerCamera::LookAtPoint(const Vec3& in_point)
	{
		// Get directional vector from camera pos to the point
		Mega::Vec3 eyeToPoint = normalize(in_point - GetPosition());

		auto euler = DirectionalVectorToEulerAngles(eyeToPoint);
		SetPitch(euler.x);
		SetYaw(-euler.y);
	}

	// ================= Getters ===================== //
	glm::vec3 EulerCamera::GetDirection() const { // TODO: make more effecient
		Vec3 out_dir = Vec3(CAMERA_FORWARD);
		out_dir = MultiplyMatrixVector(MakeRotationMatrixX(m_pitch), out_dir);
		out_dir = MultiplyMatrixVector(MakeRotationMatrixY(m_yaw), out_dir);

		return out_dir;
	}
	glm::vec3 EulerCamera::GetUp() const {
		Vec3 out_up = Vec3(CAMERA_UP);
		out_up = MultiplyMatrixVector(MakeRotationMatrixX(m_pitch), out_up);
		out_up = MultiplyMatrixVector(MakeRotationMatrixY(m_yaw),  out_up);

		return out_up;
	}

	void EulerCamera::Forward(const float in_movement) {
		Vec3 vec = GetDirection();

		Move(vec * in_movement);
	}
	void EulerCamera::Strafe(const float in_movement) {
		Vec3 dir = GetDirection();
		Vec3 vec = MakeVec3(dir.z, 0.0f, -dir.x);

		Move(vec * in_movement);
	}
	void EulerCamera::Up(const float in_movement) {
		Vec3 up = GetUp();

		Move(up * in_movement);
	}
}

// Matrix helper functions
glm::vec3 MakeVec3(float x, float y, float z) {
	glm::vec3 out_v;
	out_v.x = x;
	out_v.y = y;
	out_v.z = z;
	return out_v;
}
glm::mat4x4 MakeMat4(float v) {
	glm::mat4x4 out_m;
	out_m[0][0] = v;
	out_m[0][1] = v;
	out_m[0][2] = v;
	out_m[0][3] = v;
	out_m[1][0] = v;
	out_m[1][1] = v;
	out_m[1][2] = v;
	out_m[1][3] = v;
	out_m[2][0] = v;
	out_m[2][1] = v;
	out_m[2][2] = v;
	out_m[2][3] = v;
	out_m[3][0] = v;
	out_m[3][1] = v;
	out_m[3][2] = v;
	out_m[3][3] = v;
	return out_m;
}

glm::mat4x4 MakeRotationMatrixX(float in_thetaRad) {
	glm::mat4x4 out_mat = MakeMat4(0.0f);
	out_mat[0][0] = 1.0f;
	out_mat[1][1] = cosf(in_thetaRad);
	out_mat[1][2] = sinf(in_thetaRad);
	out_mat[2][1] = -sinf(in_thetaRad);
	out_mat[2][2] = cosf(in_thetaRad);
	out_mat[3][3] = 1.0f;
	return out_mat;
}
glm::mat4x4 MakeRotationMatrixY(float in_thetaRad) {
	glm::mat4x4 out_mat = MakeMat4(0.0f);;
	out_mat[0][0] = cosf(in_thetaRad);
	out_mat[0][2] = sinf(in_thetaRad);
	out_mat[2][0] = -sinf(in_thetaRad);
	out_mat[1][1] = 1.0f;
	out_mat[2][2] = cosf(in_thetaRad);
	out_mat[3][3] = 1.0f;
	return out_mat;
}
glm::mat4x4 MakeRotationMatrixZ(float in_thetaRad) {
	glm::mat4x4 out_mat;
	out_mat[0][0] = cos(in_thetaRad);
	out_mat[0][1] = sinf(in_thetaRad);
	out_mat[1][0] = -sinf(in_thetaRad);
	out_mat[1][1] = cosf(in_thetaRad);
	out_mat[2][2] = 1.0f;
	out_mat[3][3] = 1.0f;
	return out_mat;
}

glm::vec3 MultiplyMatrixVector(const glm::mat4x4& m, const glm::vec3& v) {
	glm::vec3 out_vec;
	out_vec.x = v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0]; // + v.w * m[3][0];
	out_vec.y = v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1]; // + v.w * m[3][1];
	out_vec.z = v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2]; // + v.w * m[3][2];
	//out_vec.w = v.x * m[0][3] + v.y * m[1][3] + v.z * m[2][3] + v.w * m[3][3];

	return out_vec;
}