#pragma once

#include <GLM/geometric.hpp>

#include <GLM/vec2.hpp>
#include <GLM/vec3.hpp>
#include <GLM/vec4.hpp>
#include <Bullet3D/LinearMath/btVector3.h>

namespace Mega
{
	using tScalarPrecision = btScalar;

	using Vec2 = glm::vec<2, tScalarPrecision, glm::packed_highp>;
	using Vec3 = glm::vec<3, tScalarPrecision, glm::packed_highp>;
	using Vec4 = glm::vec<4, tScalarPrecision, glm::packed_highp>;

	using Vec2U = glm::vec<2, unsigned int, glm::packed_highp>;
	using Vec3U = glm::vec<3, unsigned int, glm::packed_highp>;
	using Vec4U = glm::vec<4, unsigned int, glm::packed_highp>;

	using Vec2I = glm::vec<2, int, glm::packed_highp>;
	using Vec3I = glm::vec<3, int, glm::packed_highp>;
	using Vec4I = glm::vec<4, int, glm::packed_highp>;

	float AngleBetweenVectors(const Vec2& in_v1, const Vec2& in_v2);

	Vec3 DirectionalVectorToEulerAngles(const Vec3& in_dir);
	Vec2 GetDirectionVector(tScalarPrecision in_yaw);
}