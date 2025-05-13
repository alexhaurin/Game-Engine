#pragma once

#include "Engine/Engine.h"

#include "Game/World/SkyBox.h"
#include "Game/Objects/Arena.h"
#include "Game/Characters/Player/Player.h"

#include "Engine/Objects/Wall.h"
#include "Engine/Objects/PointLight.h"
#include "Engine/Objects/SphereLight.h"
#include "Engine/Objects/DirectionalLight.h"

class World : public Mega::Entity
{
public:
	void OnInitialize() override;
	void OnDestroy() override;
	void OnUpdate(const Mega::tTimestep in_dt) override;

private:
	Player* m_pPlayer = nullptr;

	Mega::Component::SoundPlayer* m_pSoundPlayer = nullptr; // plays global sounds and music
	Mega::PointLight* pointLight = nullptr;
	Mega::DirectionalLight* dirLight = nullptr;
};