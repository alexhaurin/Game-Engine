#pragma once

#include "Vector.h"

namespace Mega
{
	struct Ray2D
	{
		Ray2D()
			: origin({ 0, 0 }), direction({ 0, 1 }) {}
		Ray2D(const Vec2& in_origin, const Vec2& in_dir)
			: origin(in_origin), direction(in_dir) {};

		Vec2 origin;
		Vec2 direction;
	};
	// Axis Aligned Bounding Box described by two of its corners ("min" or bottom corner, and "max" or top corner)
	struct AABB
	{
		AABB(const Vec2& in_min, const Vec2& in_max)
			: min(in_min), max(in_max) {};

		Vec2 min;
		Vec2 max;
	};

	// Returns 0 if no collision, and distance is there is a collision
	float CheckCollision_RayAABB(const Ray2D& in_ray, const AABB& in_box);
	// Returns the direction of the new reflected ray. NOTE: you must still update the origin of the ray to the collision
	// point, this just returns the direction
	Vec2 ReflectRay(const Ray2D& in_ray, const Vec2& in_normal);
} // namespace Mega