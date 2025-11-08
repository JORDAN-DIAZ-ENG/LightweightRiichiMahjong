#pragma once

#include "Tile.h"
#include "Player.h"

#include <random>

namespace LRMahjong::Model
{
	class GameState
	{
	public:
		GameState();
		Tile wall[136]; // Full wall of tiles
		Player players[4]; // Four players


	private:


	};

} // namespace LRMahjong