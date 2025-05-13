#pragma once

#include "Engine/Engine.h"

class EnemyAISystem : public Mega::System
{
public:
	EnemyAISystem() = default;

	Mega::eMegaResult OnInitialize() override { return Mega::eMegaResult::SUCCESS; };

	Mega::eMegaResult OnUpdate(const Mega::tTimestep in_dt, Mega::Scene in_pScene) override
	{
		//ImGui::Text("Enemy AI Working"); return Mega::eMegaResult::SUCCESS;
	};

	Mega::eMegaResult OnDestroy() override { return Mega::eMegaResult::SUCCESS; };

private:

};