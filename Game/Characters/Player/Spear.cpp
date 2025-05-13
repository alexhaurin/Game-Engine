#include "Spear.h"

void Spear::OnInitialize()
{
	AddComponent<Mega::Component::Model>(Mega::Engine::LoadOBJ("Assets/Models/Shapes/Rect.obj"));
	(&GetComponent<Mega::Component::Transform>())->SetScale(Mega::Vec3(5, 1, 1));
}

void Spear::OnDestroy()
{

}

void Spear::OnUpdatePost(const Mega::tTimestep in_dt)
{

}