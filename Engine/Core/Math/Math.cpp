#include "Math.h"

#include <ctime>
#include <iostream>

namespace Mega
{
	// ===================== Randomness ====================== //
    // https://stackoverflow.com/questions/686353/random-float-number-generation
	// Uniform, but still lower percision because RAND_MAX is pretty low compared to FLOAT_MAX
	float RandomFloat(float in_min, float in_max)
	{
		return in_min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (in_max - in_min)));
	}

	bool Chance(int in_percent)
	{
		return (rand() % 100 <= in_percent);
	}

	// =================== Interpolation ====================== //
	// https://solhsa.com/interpolation/
	// returns a value between 0 and 1 (start and end) using and squared deceleration slerp
	float SquaredAcceleration(float in_weight)
	{
		MEGA_ASSERT((in_weight <= 1.0f && in_weight >= 0.0f), "Slerp weight value not between 0 and 1");
		float start = 0.0f;
		float end = 1.0f;

		float v = 1 - (1 - in_weight) * (1 - in_weight);
		return ((start * v) + (end * (1 - v)));
	}
	float CubedAcceleration(float in_weight)
	{
		MEGA_ASSERT((in_weight <= 1.0f && in_weight >= 0.0f), "Slerp weight value not between 0 and 1");
		float start = 0.0f;
		float end = 1.0f;

		float v = 1 - (1 - in_weight) * (1 - in_weight) * (1 - in_weight);
		return ((start * v) + (end * (1 - v)));
	}
}