#pragma once

#include "Engine/Engine.h"

namespace Mega
{
	class SphereLight : public Entity
	{
	public:
		SphereLight(const Vec3& in_pos, float in_radius = 1.0f)
			: m_position(in_pos), m_radius(in_radius) {};

		void OnInitialize() override
		{
			// Set light data
			m_pLightData = &AddComponent<Component::Light>(m_position, m_radius).lightData;
			m_pLightData->type = eLightTypes::Sphere;
			m_pLightData->radius = m_radius;
			m_pLightData->strength = m_radius;

			// Just for scene drawing
			AddComponent<Component::Model>(Mega::Engine::LoadOBJ("Assets/Models/Shapes/UVSphere.obj"));
			//AddComponent<CollisionSphereComponent>(m_radius);
			//AddComponent<RigidBodyComponent>();
			GetComponent<Component::Model>().materialData = MaterialData(0.1f, 0.1f, 1.0f, 1.0f);

			SetPosition(m_position);
			SetScale(Vec3(m_radius));
		}

		void OnUpdate(const tTimestep in_dt) override
		{
			m_pLightData->position = GetPosition();
		}

		LightData* GetLightData() { return m_pLightData; }

	private:
		LightData* m_pLightData = nullptr;

		Vec3 m_position = { 0, 0, 0 };
		float m_radius = 1.0f;
	};
}