#pragma once

#include "Vector.h"

#include <GLM/matrix.hpp>
#include <GLM/gtx/quaternion.hpp>

namespace Mega
{
	using Mat4x4 = glm::mat<4, 4, tScalarPrecision, glm::packed_highp>;
	using Mat3x3F = glm::mat<4, 4, tScalarPrecision, glm::packed_highp>;
}