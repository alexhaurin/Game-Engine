#include "Collisions2D.h"

#include <minmax.h>
#include <iostream>

namespace Mega
{
    // Branchless aabb raycast algorithm courtesy of https://tavianator.com/2011/ray_box.html
    // For now assuming in_ray's direction is normalized
	float CheckCollision_RayAABB(const Ray2D& in_ray, const AABB& in_box)
	{
        Mega::Vec2 n_inv = 1.0f / glm::normalize(in_ray.direction); // normalized inverse

        float tx1 = (in_box.min.x - in_ray.origin.x) * n_inv.x;
        float tx2 = (in_box.max.x - in_ray.origin.x) * n_inv.x;
        
        float tmin = min(tx1, tx2);
        float tmax = max(tx1, tx2);
        
        float ty1 = (in_box.min.y - in_ray.origin.y) * n_inv.y;
        float ty2 = (in_box.max.y - in_ray.origin.y) * n_inv.y;

        tmin = max(tmin, min(ty1, ty2));
        tmax = min(tmax, max(ty1, ty2));

        if (tmax >= tmin && tmax > 0) // If collision is detected
        {
            return tmin; // return distance to collision, negative means inside of aabb
        }
        return 0.0f;
	}

    Vec2 ReflectRay(const Ray2D& in_ray, const Vec2& in_normal)
    {
        float dot = glm::dot(in_ray.direction, in_normal);
        return in_ray.direction - in_normal * (dot * 2.0f); // new direction of vector;
    }

}