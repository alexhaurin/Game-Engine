#pragma once

#include "Engine/Engine.h"

class Game
{
public:
	void Initialize();
	void Run();
	void Destroy();

private:
	Mega::Scene* m_pScene = nullptr;
};