#pragma once

#include "Engine/Engine.h"

namespace Mega
{
	class PointLight : public Entity
	{
	public:
		PointLight(const Vec3& in_pos, float in_strength = 1.0f)
			: m_position(in_pos), m_strength(in_strength) {};

		void OnInitialize() override
		{
			// Set light data
			m_pLightData = &AddComponent<Component::Light>(m_position, m_strength).lightData;
			m_pLightData->type = eLightTypes::Point;
			m_pLightData->strength = m_strength;

			// Just for scene drawing
			AddComponent<Component::Model>(Mega::Engine::LoadOBJ("Assets/Models/Shapes/UVSphere.obj"));
			GetComponent<Component::Model>().materialData = MaterialData(0.1f, 0.1f, 1.0f, 1.0f);
			SetLightPosition(m_position);
		}

		void OnUpdate(const tTimestep in_dt) override
		{
			m_pLightData->position = GetLightPosition();
		}

		void SetLightPosition(const Mega::Vec3& in_pos) { m_position = in_pos; m_pLightData->position = in_pos; SetPosition(m_position); }
		Mega::Vec3 GetLightPosition() const { return m_position; }
		LightData* GetLightData() { return m_pLightData; }

	private:
		LightData* m_pLightData = nullptr;

		Vec3 m_position = { 0, 0, 0 };
		float m_strength = 1.0f;
	};
}