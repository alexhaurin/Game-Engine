#pragma once

#include "Engine/Engine.h"

class Spear : public Mega::Entity
{
public:
	Spear() = default;

	void OnInitialize() override;
	void OnDestroy() override;
	void OnUpdatePost(const Mega::tTimestep in_dt) override;

	void SetPosition(const Mega::Vec3& in_pos) { GetComponent<Mega::Component::Transform>().SetPosition(in_pos); }

	Mega::Component::Transform* GetTransformComponent() { return &GetComponent<Mega::Component::Transform>(); }

private:

};