#pragma once

#include "Engine/Engine.h"

namespace Mega
{
	class DirectionalLight : public Entity
	{
	public:
		DirectionalLight(const Vec3& in_dir, float in_strength = 1.0f)
			: m_direction(in_dir), m_strength(in_strength) {};

		void OnInitialize() override
		{
			// Set light data
			m_pLightData = &AddComponent<Component::Light>(Vec3(0, 0, 0), m_strength).lightData;
			m_pLightData->type = eLightTypes::Directional;
			m_pLightData->strength = m_strength;
			m_pLightData->direction = m_direction;

			// Just for scene drawing
			SetLightDirection(m_direction);
			SetPosition({ 0, 0, 0 }); // Setting ENTITY position
		}

		void OnUpdate(const tTimestep in_dt) override
		{
			m_pLightData->direction = GetLightDirection();
		}

		void SetLightDirection(const Mega::Vec3& in_dir) { m_direction = in_dir; m_pLightData->direction = in_dir; }
		Mega::Vec3 GetLightDirection() const { return m_direction; }
		LightData* GetLightData() { return m_pLightData; }

	private:
		LightData* m_pLightData = nullptr;

		Vec3 m_direction = { 1, 0, 0 };
		float m_strength = 1.0f;
	};
} // namespace Mega