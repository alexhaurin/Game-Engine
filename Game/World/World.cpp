#include "World.h"

void World::OnInitialize()
{
	m_pSoundPlayer = &AddComponent<Mega::Component::SoundPlayer>(); // "Walking", Mega::Engine::LoadSound("Assets/Sounds/test.wav"));
	m_pSoundPlayer->AddSoundEffect("Ambient", Mega::Engine::LoadSound("Assets/Sounds/ambientOutside.wav"));
	m_pSoundPlayer->SetGain(1.5f);

	// TODO: only scene should have control over entities (scene cnotrols the root)
	pointLight = Mega::Engine::AddChildEntity<Mega::PointLight>(this, Mega::Vec3(-50, 4, -5), -13);
	pointLight->GetLightData()->color = { 1, 0, 0 };
	pointLight->GetLightData()->strength = 10.0;

	pointLight = Mega::Engine::AddChildEntity<Mega::PointLight>(this, Mega::Vec3(40, 8, 9), -13);
	pointLight->GetLightData()->color = { 1, 0, 1 };
	pointLight->GetLightData()->strength = 10.0;

	dirLight = Mega::Engine::AddChildEntity<Mega::DirectionalLight>(this, Mega::Vec3(-1, 5, -1), 5); // Last one is shadow map directional light
	dirLight->GetLightData()->color = { 1, 1, 1 };

	m_pPlayer = Mega::Engine::AddChildEntity<Player>(this); // Main player
	auto* w = Mega::Engine::AddChildEntity<Mega::Wall>(this, Mega::Vec3(0, -0.5, 0), Mega::Vec3(300, 1, 300), Mega::Vec3(0, 0, 0)); // Ground
	w->SetTexture(Mega::Engine::LoadTexture("Assets/Textures/GrassTextureMap.png"));
	//Mega::Engine::AddChildEntity<Mega::Wall>(this, Mega::Vec3(0, -10, 10), Mega::Vec3(20, 20, 20), Mega::Vec3(0, 0, 0.3)); // Block

	//Mega::Engine::AddChildEntity<SkyBox>(this);
	Mega::Engine::AddChildEntity<Arena>(this);
}

void World::OnUpdate(const Mega::tTimestep in_dt)
{
	m_pSoundPlayer->Play("Ambient");

	Mega::Vec3 lightDir = dirLight->GetLightDirection();
	ImGui::DragFloat3("Sun Color", &dirLight->GetLightData()->color.x, 0.001, 0.0, 1.0);
	ImGui::DragFloat("Sun Strength", &dirLight->GetLightData()->strength, 0.01, 0.0, 1.0);
	ImGui::DragFloat3("Sun Direction", &lightDir.x, 1);
	dirLight->SetLightDirection(lightDir);
}

void World::OnDestroy()
{

}