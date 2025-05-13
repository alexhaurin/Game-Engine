#include "Components.h"

namespace Mega
{
	namespace Component
	{
		// -------------- Transform ----------------- //
		Transform::Transform(const Vec3& in_pos)
		{
			SetPosition(in_pos);
		}
		Transform::Transform(const Vec3& in_pos, const Vec3& in_rot, const Vec3& in_scale)
		{
			SetRotation(in_rot);
			SetPosition(in_pos);
			SetScale(in_scale);
		}
		void Transform::SetRotation(const Mat4x4& in_rot)
		{
			transform = Mat4x4(1.0f);

			SetScale(scale);
			transform *= in_rot;
			SetPosition(translation);
		}
		void Transform::SetRotation(const Vec3& in_rot)
		{
			rotation = in_rot;
			transform = Mat4x4(1.0f);

			SetScale(scale);
			transform = glm::rotate(transform, in_rot.x, Vec3(1, 0, 0));
			transform = glm::rotate(transform, in_rot.y, Vec3(0, 1, 0));
			transform = glm::rotate(transform, in_rot.z, Vec3(0, 0, 1));
			SetPosition(translation);
		}
		void Transform::SetPosition(const Vec3& in_pos)
		{
			translation = in_pos;

			transform[3][0] = in_pos.x;
			transform[3][1] = in_pos.y;
			transform[3][2] = in_pos.z;
		}
		void Transform::SetScale(const Vec3& in_scale)
		{
			scale = in_scale;

			transform[0][0] = in_scale.x;
			transform[1][1] = in_scale.y;
			transform[2][2] = in_scale.z;
		}
	} // namespace Component
} // namespace Mega