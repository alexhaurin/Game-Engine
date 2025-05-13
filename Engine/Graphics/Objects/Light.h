#pragma once

#include "Engine/Core/Math/Vector.h"

namespace Mega
{
	enum class eLightTypes {
		Point = 0,
		Directional = 1,
		Sphere = 2,
	};

	struct LightData {
		using glScalarF = float;
		using glScalarUI = uint32_t;

		alignas(sizeof(glScalarUI)) eLightTypes type = eLightTypes::Point;

		glScalarF strength = 1.0f;
		glScalarF radius = 1.0f;

		glScalarF inCutOff = 0.9f;
		glScalarF outCutOff = 0.8f;
		glScalarF PADDING1;
		glScalarF PADDING2;
		glScalarF PADDING3;

		alignas(sizeof(glScalarF) * 4)  Vec3 position = Vec3(1.0f, 1.0f, 1.0f);
		alignas(sizeof(glScalarF) * 4)  Vec3 direction = Vec3(0.5f, 1.0f, 0.0f);
		alignas(sizeof(glScalarUI) * 4) Vec3 color = Vec3(1.0, 1.0, 1.0);
	};
}