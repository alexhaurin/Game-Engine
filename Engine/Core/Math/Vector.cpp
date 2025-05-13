#include "Vector.h"

#include "Engine/Core/Math/Math.h"

float Mega::AngleBetweenVectors(const Mega::Vec2& in_v1, const Mega::Vec2& in_v2)
{
	auto v1 = glm::normalize(in_v1);
	auto v2 = glm::normalize(in_v2);
	tScalarPrecision dot = glm::dot(v1, v2);
	tScalarPrecision det = (float)v1.x * v2.y - (float)v1.y * v2.x; // determinent
	return atan2(det, dot);
}

Mega::Vec3 Mega::DirectionalVectorToEulerAngles(const Mega::Vec3& in_dir)
{
	Vec3 dir = glm::normalize(in_dir);

	// yaw
	tScalarPrecision yaw = atan2(-dir.x, -dir.z);

	// pitch
	tScalarPrecision dist = std::sqrt((in_dir.x * in_dir.x) + (in_dir.z * in_dir.z));
	tScalarPrecision pitch = std::atan2(in_dir.y, dist);

	return { pitch, yaw, 0 };
}

// Yaw pitch roll to directional vector
Mega::Vec2 Mega::GetDirectionVector(tScalarPrecision in_yaw)
{
	return { sin(in_yaw), cos(in_yaw) };
}