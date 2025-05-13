#include "PhysicsComponents.h"

namespace Mega
{
	namespace Component
	{
		Mat4x4 RigidBody::GetMotionStateTransform() const
		{
			btTransform t;
			pMotionState->getWorldTransform(t);

			Mat4x4 out_t;
			t.getOpenGLMatrix((float*)&out_t);
			return out_t;
		}
		Vec3 RigidBody::GetMotionStatePosition() const
		{
			btTransform t;
			pMotionState->getWorldTransform(t);
			btVector3 bt = t.getOrigin();
			return Vec3(bt.getX(), bt.getY(), bt.getZ());
		}
		Vec3 RigidBody::GetLinearVelocity() const
		{
			const btVector3& vec = pPhysicsBody->getLinearVelocity();
			return { vec.x(), vec.y(), vec.z() };
		}
		void RigidBody::SyncRotation(bool in_bool)
		{
			syncRot = in_bool;
			if (syncRot) { pPhysicsBody->setAngularFactor({ 1, 1, 1 }); }
			else { pPhysicsBody->setAngularFactor({ 0, 0, 0 }); }
		}
	} // namespace Component
} // namespace Mega