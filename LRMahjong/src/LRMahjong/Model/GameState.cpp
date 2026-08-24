#include "GameState.h"

namespace LRMahjong::Model
{
	namespace
	{
		constexpr uint8_t DORA_SLOT_BASE        = 0;
		constexpr uint8_t URA_SLOT_BASE         = 5;
		constexpr uint8_t REPLACEMENT_SLOT_BASE = 10;
		constexpr uint8_t MAX_REPLACEMENTS      = 4;
	}

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

	TileInstance GameState::DoraIndicator( const uint8_t i ) const
	{
		if ( i >= doraIndicators ) return INVALID_INSTANCE;
		return wall[DeadWallStart() + DORA_SLOT_BASE + i];
	}

	TileInstance GameState::UraIndicator( const uint8_t i ) const
	{
		if ( i >= doraIndicators ) return INVALID_INSTANCE;
		return wall[DeadWallStart() + URA_SLOT_BASE + i];
	}

	TileInstance GameState::DrawLive()
	{
		if ( LiveWallEmpty() ) return INVALID_INSTANCE;
		return wall[liveWallHead++];
	}

	TileInstance GameState::DrawReplacement()
	{
		if ( deadWallDraws >= MAX_REPLACEMENTS ) return INVALID_INSTANCE;

		const TileInstance tile = wall[DeadWallStart() + REPLACEMENT_SLOT_BASE + deadWallDraws];
		++deadWallDraws;

		// The dead wall is topped up from the back of the live wall, which is
		// what costs the hand one draw per kan.
		if ( liveWallTail > liveWallHead ) --liveWallTail;

		return tile;
	}

	void GameState::RevealDoraIndicator()
	{
		if ( doraIndicators < MAX_DORA_INDICATORS ) ++doraIndicators;
	}

	uint8_t GameState::TotalKans() const
	{
		uint8_t kans = 0;
		for ( uint8_t seat = 0; seat < rules.numPlayers; ++seat )
		{
			for ( uint8_t i = 0; i < players[seat].meldCount; ++i )
			{
				if ( players[seat].melds[i].IsKan() ) ++kans;
			}
		}
		return kans;
	}

	Counts34 GameState::VisibleTo( const uint8_t viewer ) const
	{
		Counts34 visible{};

		// The drawn tile lives in the hand histogram, so counting the hand
		// already counts it.
		if ( viewer < rules.numPlayers )
		{
			const Counts34 &own = players[viewer].hand.Counts();
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				visible[t] = static_cast<uint8_t>( visible[t] + own[t] );
			}
		}

		for ( uint8_t seat = 0; seat < rules.numPlayers; ++seat )
		{
			players[seat].AddRevealedTo( visible );
		}

		for ( uint8_t i = 0; i < doraIndicators; ++i )
		{
			const TileId t = InstanceTile( DoraIndicator( i ) );
			if ( IsValidTile( t ) ) ++visible[t];
		}

		return visible;
	}

} // namespace LRMahjong::Model
