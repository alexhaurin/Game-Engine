#pragma once

#include "EulerCamera.h"

namespace Mega
{
	class OrthographicCamera : public EulerCamera
	{
	public:
		OrthographicCamera() = default;
		OrthographicCamera(tScalar in_left, tScalar in_right, tScalar in_top, tScalar in_bottom, tScalar in_near = -1.0f, tScalar in_far = 1.0f);

		virtual Mat4x4 GetLookAtMatrix() const override;
		//virtual Mat4x4 GetProjection(tScalar in_FOVDegrees, tScalar in_aspectRation, tScalar in_near, tScalar in_far) const override;

	private:
		tScalar m_left = -1000;
		tScalar m_right = 1000;
		tScalar m_top = -1000;
		tScalar m_bottom = 1000;
		tScalar m_near = -1;
		tScalar m_far = 1;
	};
} // namespace Mega