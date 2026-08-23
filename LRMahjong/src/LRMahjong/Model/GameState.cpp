#include "GameState.h"

namespace LRMahjong::Model
{
	void GameState::Reset( const Rules &newRules, const uint8_t newDealer )
	{
		*this = GameState{};

		rules  = newRules;
		dealer = newDealer;

		wallCount     = static_cast<uint8_t>( rules.TotalTiles() );
		liveWallHead  = 0;
		liveWallTail  = static_cast<uint8_t>( wallCount - DEAD_WALL_TILES );
		currentPlayer = newDealer;
		phase         = Phase::DEAL;

		for ( uint8_t seat = 0; seat < rules.numPlayers; ++seat )
		{
			Player &p = players[seat];
			p.points   = rules.startingPoints;
			// East is the dealer; winds run anticlockwise from there.
			p.seatWind = static_cast<uint8_t>( ( seat + rules.numPlayers - newDealer ) % rules.numPlayers );
		}
	}

	Counts34 GameState::VisibleTo( const uint8_t viewer ) const
	{
		Counts34 visible{};

		if ( viewer < rules.numPlayers )
		{
			const Counts34 &own = players[viewer].hand.Counts();
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				visible[t] = static_cast<uint8_t>( visible[t] + own[t] );
			}

			const TileId drawn = InstanceTile( players[viewer].drawn );
			if ( IsValidTile( drawn ) ) ++visible[drawn];
		}

		for ( uint8_t seat = 0; seat < rules.numPlayers; ++seat )
		{
			players[seat].AddRevealedTo( visible );
		}

		// Face-up dora indicators sit at the front of the dead wall.
		for ( uint8_t i = 0; i < doraIndicators; ++i )
		{
			const TileId t = InstanceTile( wall[liveWallTail + i] );
			if ( IsValidTile( t ) ) ++visible[t];
		}

		return visible;
	}

} // namespace LRMahjong::Model
