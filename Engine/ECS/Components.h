#pragma once

#include <vector>
#include <iostream>

#include <Bullet3D/LinearMath/btScalar.h>
#include <Bullet3D/LinearMath/btVector3.h>
#include <Bullet3D/LinearMath/btMatrix3x3.h>
#include <Bullet3D/LinearMath/btTransform.h>
#include <Bullet3D/btBulletDynamicsCommon.h>
#include <Bullet3D/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

#include "Engine/Core/Core.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Sound/SoundData.h"
#include "Engine/Sound/SoundSource.h"

namespace Mega
{
	namespace Component
	{
		struct ComponentBase {}; // Pure virtual parent class

		// Transform
		struct Transform : public ComponentBase
		{
		public:
			using tScalar = Mega::tScalarPrecision;

			Transform() = default;
			Transform(const Vec3& in_pos);
			Transform(const Vec3& in_pos, const Vec3& in_rot, const Vec3& in_scale);

			void SetTransform(const Mat4x4& in_rot) { transform = in_rot; }
			Mat4x4 GetTransform() const { return transform; }
			Vec3 GetPosition() const { return translation; }
			Vec3 GetScale() const { return scale; }

			Vec3 GetRotation() const { return rotation; }
			void SetRotation(const Mat4x4& in_rot);
			void SetRotation(const Vec3& in_rot);
			void SetPosition(const Vec3& in_pos);
			void SetScale(const Vec3& in_scale);

		private:
			Mat4x4 transform = Mat4x4(1.0f);
			Vec3 translation = Vec3(0.0f);
			Vec3 rotation = Vec3(0.0f);
			Vec3 scale = Vec3(1.0f);

			operator glm::mat4() { return GetTransform(); }
			operator const glm::mat4()& { return GetTransform(); }
		};
	} // namespace Component
} // namespace Mega