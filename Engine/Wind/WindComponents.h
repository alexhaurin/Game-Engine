#pragma once

#include "Engine/Wind/WindSystem.h"
#include "Engine/ECS/Components.h"
#include "Engine/Core/Math/Math.h"

namespace Mega
{
	namespace Component
	{
		struct WindMotor : public ComponentBase
		{
		public:
			friend WindSystem;

			void Directional(const Vec3& in_direction, const WindSystem::tScalar in_force);
			void SetPosition(const Vec3& in_position) { position = in_position; }

		private:
			Vec3 position = { 0, 0, 0 };

			bool isOn = false;
			Vec3 fanDirection = { 0, 0, 0 };
			Vec3 forceVector;
			WindSystem::tScalar fanForce = { 0 };
		};
	
		// Add to an entity to read the current wind at a specific position during each frame
		struct WindReciever : public ComponentBase
		{
		public:
			friend WindSystem;

			void SetPosition(const Vec3& in_position) { position = in_position; }
			void AddWindData(const Vec3& in_d) {};

		private:
			Vec3 position = { 0, 0, 0 };
			Vec4 windAtPosition = { 0, 0, 0, 0 }; //x, y, z, magnitude
		};
	}
}

