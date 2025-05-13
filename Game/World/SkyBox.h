#pragma once

#include "Engine/Engine.h"

class SkyBox : public Mega::Entity
{
public:
	void OnInitialize() override
	{
		SetScale(Mega::Vec3(300));

		AddComponent<Mega::Component::Model>(
			Mega::Engine::LoadOBJ("Assets/Models/SkySphere.obj"),
			Mega::Engine::LoadTexture("Assets/Textures/SkyBox.jpg"),
			Mega::MaterialData(0, 1, 0, 1)
		);

		// SetScale(m_dimensions);
	}
};