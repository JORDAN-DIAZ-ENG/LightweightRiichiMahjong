#include "Engine.h"

#include <stdio.h>
#include <iostream>

#include "Model/GameState.h"

namespace LRMahjong
{

	using namespace Model;

	void Engine::Start()
	{
		// Main engine loop implementation
		std::cout << R"(
    __    _       __    __                _       __    __            
   / /   (_)___ _/ /_  / /__      _____  (_)___ _/ /_  / /_           
  / /   / / __ `/ __ \/ __/ | /| / / _ \/ / __ `/ __ \/ __/           
 / /___/ / /_/ / / / / /_ | |/ |/ /  __/ / /_/ / / / / /_             
/_____/_/\__, /_/ /_/\__/ |__/|__/\___/_/\__, /_/ /_/\__/             
    ____/____/     __    _    __  ___   /____/     _                  
   / __ \(_|_)____/ /_  (_)  /  |/  /___ _/ /_    (_)___  ____  ____ _
  / /_/ / / / ___/ __ \/ /  / /|_/ / __ `/ __ \  / / __ \/ __ \/ __ `/
 / _, _/ / / /__/ / / / /  / /  / / /_/ / / / / / / /_/ / / / / /_/ /
/_/ |_/_/_/\___/_/ /_/_/  /_/  /_/\__,_/_/ /_/_/ /\____/_/ /_/\__, /  
                                            /___/            /____/
    )" << std::endl;

		GameState game;

		std::cout << "Engine started." << std::endl;
		std::cout << "\nPlayer One's Turn." << std::endl;
		game.players[0].hand.PrintHand();
		std::cout << "\n\nPlayer Two's Turn." << std::endl;
		game.players[1].hand.PrintHand();
		std::cout << "\n\nPlayer Three's Turn." << std::endl;
		game.players[2].hand.PrintHand();
		std::cout << "\n\nPlayer Four's Turn." << std::endl;
		game.players[3].hand.PrintHand();
		std::cout << "\n";

	}

} // namespace LRMahjong