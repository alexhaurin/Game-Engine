#pragma once

#include <cmath>
#include <chrono>
#include <cstdint>

#include "Matrix.h"
#include "Vector.h"
#include "Collisions2D.h"
#include "Engine/Core/Debug.h"

#define PI_F 3.1415927410125732421875f

namespace Mega
{
	// ========= Randomness =========== //
	float RandomFloat(float in_min, float in_max); // Returns a float between the given min and max (inclusive)
	bool Chance(int in_percent);

	// ===== Interpolations ========= //
	float SquaredAcceleration(float in_weight);
	float CubedAcceleration(float in_weight);
}