#include "Game/Game.h"

#include <thread>
#include <time.h>

#include "Game/World/World.h"

// TODO: Another name for scene? or just have "engine" take care of the systems part and have
// scene just control the entities/loaded shit? (scene would basically be root entity)
// TODO: Entity (Player) GetPosition() should use rigid body motionstate not entity transform?
// TODO: fix transform component class (glm::decompose) - or really transform should only control pos, rot, scale, and make proper 
// transform on GetTransform()
// TODO: Animation model matrices are now getting their entity's transform directly multiplied to them cpu side
// (to make joint attachment and stuff easier) is this efficient enough?

void Game::Initialize()
{
    Mega::Engine::Initialize();

    m_pScene = Mega::Engine::GetScene();
    m_pScene->CreateRootEntity<World>();
}   

// Game loop
void Game::Run()
{
    Mega::tTimestep dt = 0.0; // Number of millis per frame
    // For now lock at 60FPS - target time is number of nanosecs one frame should take
    Mega::tNanosecond targetTime = Mega::tNanosecond((int)((1000.0 * 1000.0 * 1000.0) / 30.0)); // TODO: lock to physics using GameState jawn
    Mega::tNanosecond timePassed = Mega::tNanosecond(0);
    Mega::tNanosecond currentTime = Mega::Time<Mega::tNanosecond>();

    while (!Mega::Engine::ShouldClose())
    {
        Mega::tNanosecond startTime = Mega::Time<Mega::tNanosecond>();

        Mega::Engine::HandleInput();
        Mega::Engine::Update(dt);
        Mega::Engine::Display();

        // Wait loop to lock at the target FPS
        Mega::tNanosecond endTime = Mega::Time<Mega::tNanosecond>();
        auto deltaTime = (endTime - startTime);
        if (deltaTime < targetTime)
        {
            Mega::tNanosecond remainingTime = targetTime - deltaTime - Mega::tMillisecond(2); // Buffer needed because the sleep function is not exact
            if (remainingTime > Mega::tNanosecond(0))
            {
                std::this_thread::sleep_for(remainingTime);
            }

            while ((Mega::Time<Mega::tNanosecond>() - startTime + Mega::tMicrosecond(10)) < targetTime) {};
        }

        dt = (Mega::Time<Mega::tNanosecond>() - startTime).count() / 1000.0 / 1000.0;
    }
}

void Game::Destroy()
{
    Mega::Engine::Destroy();
}