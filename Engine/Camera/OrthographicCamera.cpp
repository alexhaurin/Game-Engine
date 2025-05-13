#include "OrthographicCamera.h"

#include <GLM/gtc/matrix_transform.hpp>

namespace Mega
{
	OrthographicCamera::OrthographicCamera(tScalar in_left, tScalar in_right, tScalar in_top, tScalar in_bottom, tScalar in_near, tScalar in_far)
	{
		m_left   = in_left;
		m_right  = in_right;
		m_top    = in_top;
		m_bottom = in_bottom;
		m_near   = in_near;
		m_far    = in_far;
	}

	Mat4x4 OrthographicCamera::GetLookAtMatrix() const
	{
		// Get the transform of the camera using positon + rotation, then invert it
		Mat4x4 out_transform(1.0f);

		out_transform = glm::translate(glm::mat4(1.0f), GetPosition());
		out_transform *= glm::rotate(glm::mat4(1.0f), GetYaw(), glm::vec3(0, 1, 0));
		out_transform *= glm::rotate(glm::mat4(1.0f), GetPitch(), glm::vec3(1, 0, 0));
		out_transform *= glm::rotate(glm::mat4(1.0f), GetRoll(), glm::vec3(0, 0, 1));

		return glm::inverse(out_transform);
	};
} // namespace Mega