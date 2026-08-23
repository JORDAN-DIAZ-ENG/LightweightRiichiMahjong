#pragma once

#include <cstdint>
#include <type_traits>

#include "Player.h"
#include "Rules.h"
#include "Tile.h"

namespace LRMahjong::Model
{
	inline constexpr uint16_t MAX_WALL_TILES = 136;

	enum class Phase : uint8_t
	{
		DEAL,      // hand not yet dealt
		DRAW,      // current player is about to draw
		DISCARD,   // current player holds a drawn tile and must act
		CALL,      // a discard is on the table, reactors may claim it
		HAND_OVER, // the hand has ended
	};

	// Full perfect-information state of one hand.
	//
	// This is the type rollouts run on, so it is a flat aggregate: fixed-size
	// arrays, no heap, no virtuals, trivially copyable. Cloning a state is a
	// memcpy of roughly 700 bytes, which is what makes millions of playouts per
	// second reachable.
	//
	// Rules travel inside the state so a state is self-describing and sanma and
	// four-player share one type.
	struct GameState
	{
		Rules rules{};

		// The whole wall in draw order. The live wall is [liveWallHead,
		// liveWallTail); the dead wall is the tail end of the array.
		TileInstance wall[MAX_WALL_TILES]{};
		uint8_t wallCount    = 0; // 108 or 136
		uint8_t liveWallHead = 0; // index of the next live draw
		uint8_t liveWallTail = 0; // one past the last live tile
		uint8_t deadWallDraws = 0; // replacement tiles taken for kan and nuki

		uint8_t doraIndicators = 0; // how many of the 5 indicators are face up

		uint8_t roundWind     = 0; // 0 = east round
		uint8_t dealer        = 0;
		uint8_t currentPlayer = 0;
		uint8_t honba         = 0;
		uint8_t riichiSticks  = 0;

		Phase phase = Phase::DEAL;

		TileInstance lastDiscard   = INVALID_INSTANCE;
		uint8_t      lastDiscarder = INVALID_SEAT;

		Player players[MAX_PLAYERS]{};

		// Sets up seats, points and wind assignment for a fresh hand. Dealing
		// the wall itself belongs to the engine ( M1 ).
		LRM_API void Reset( const Rules &newRules, uint8_t newDealer );

		uint8_t LiveWallRemaining() const
		{
			return ( liveWallHead < liveWallTail ) ? static_cast<uint8_t>( liveWallTail - liveWallHead ) : static_cast<uint8_t>( 0 );
		}

		uint8_t PlayerCount() const { return rules.numPlayers; }

		// Every tile the given seat can see: its own hand, all melds, all
		// discards, pulled Norths and the face-up dora indicators. The
		// complement of this is the pool the observation layer samples from.
		LRM_API Counts34 VisibleTo( uint8_t viewer ) const;
	};

	// A rollout copies this type; if either assertion ever fails, the copy has
	// stopped being a memcpy and the search budget has quietly changed.
	static_assert( std::is_trivially_copyable_v<GameState>, "GameState must stay memcpy-able for rollouts" );
	static_assert( sizeof( GameState ) <= 1024, "GameState has outgrown its 1 KB budget" );

} // namespace LRMahjong::Model
