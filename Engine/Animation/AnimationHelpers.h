#pragma once

#include "Engine/Core/Math/Matrix.h"
#include "ozz/base/maths/soa_float4x4.h"

namespace Mega
{
	// Converts and Ozz Animation 4x4 Matrix to 
	inline glm::mat4x4 OzzToGLM(const ozz::math::Float4x4& m)
	{
		glm::mat4x4 out_glm =
		{
			m.cols[0].m128_f32[0], m.cols[0].m128_f32[1], m.cols[0].m128_f32[2], m.cols[0].m128_f32[3],
			m.cols[1].m128_f32[0], m.cols[1].m128_f32[1], m.cols[1].m128_f32[2], m.cols[1].m128_f32[3],
			m.cols[2].m128_f32[0], m.cols[2].m128_f32[1], m.cols[2].m128_f32[2], m.cols[2].m128_f32[3],
			m.cols[3].m128_f32[0], m.cols[3].m128_f32[1], m.cols[3].m128_f32[2], m.cols[3].m128_f32[3],
		};

		return out_glm;
	}

	inline ozz::math::Float4x4 GLMToOzz(const glm::mat4x4& m)
	{
		ozz::math::Float4x4 out_ozz{};

		out_ozz.cols[0].m128_f32[0] = m[0][0];
		out_ozz.cols[0].m128_f32[1] = m[0][1];
		out_ozz.cols[0].m128_f32[2] = m[0][2];
		out_ozz.cols[0].m128_f32[3] = m[0][3];

		out_ozz.cols[1].m128_f32[0] = m[1][0];
		out_ozz.cols[1].m128_f32[1] = m[1][1];
		out_ozz.cols[1].m128_f32[2] = m[1][2];
		out_ozz.cols[1].m128_f32[3] = m[1][3];

		out_ozz.cols[2].m128_f32[0] = m[2][0];
		out_ozz.cols[2].m128_f32[1] = m[2][1];
		out_ozz.cols[2].m128_f32[2] = m[2][2];
		out_ozz.cols[2].m128_f32[3] = m[2][3];

		out_ozz.cols[3].m128_f32[0] = m[3][0];
		out_ozz.cols[3].m128_f32[1] = m[3][1];
		out_ozz.cols[3].m128_f32[2] = m[3][2];
		out_ozz.cols[3].m128_f32[3] = m[3][3];

		return out_ozz;
	}

	inline void PrintMatrix(const glm::mat4& matrix)
	{
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				std::cout << std::setw(10) << matrix[i][j] << " ";
			}
			std::cout << std::endl;
		}
	}

} // namespace Mega