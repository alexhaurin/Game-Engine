#include "WindComponents.h"

namespace Mega
{
	namespace Component
	{
		void WindMotor::Directional(const Vec3& in_direction, const WindSystem::tScalar in_force)
		{
			isOn = true;
			fanDirection = glm::normalize(in_direction);
			fanForce = in_force;
			forceVector = fanDirection * fanForce;
		}
		// TODO: void WindMotor::Omni(const WindSystem::tScalar in_force);
	} // namespace Component
} // namespace Mega