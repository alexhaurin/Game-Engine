#pragma once

#include "Engine/ECS/ECS.h"
#include "Engine/Scene/Scene.h"

class Arena : public Mega::Entity
{
public:

	void OnInitialize() override
	{
		SetScale(Mega::Vec3(1.0));
		SetPosition(Mega::Vec3(0, -1, 0));

		auto* model = &AddComponent<Mega::Component::Model>(Mega::Engine::LoadOBJ("Assets/Models/Arena.obj"));
		AddComponent<Mega::Component::CollisionTriangleMesh>(model->vertexData);
		AddComponent<Mega::Component::RigidBody>(Mega::PhysicsSystem::eRigidBodyType::Static, 0.0f, 0.5f, 0.5f);

	};
};
