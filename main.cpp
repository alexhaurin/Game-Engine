#include "Game/Game.h"

int main()
{
    Game* game = new Game();

    game->Initialize();
    game->Run();
    game->Destroy();

    delete game;

    return EXIT_SUCCESS;
}