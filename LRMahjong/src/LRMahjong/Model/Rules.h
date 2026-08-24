#pragma once

#include <cstdint>

#include "Tile.h"

namespace LRMahjong::Model
{
	inline constexpr uint8_t MAX_PLAYERS      = 4;
	inline constexpr uint8_t DEAD_WALL_TILES  = 14;
	inline constexpr uint8_t MAX_DORA_INDICATORS = 5;
	inline constexpr uint8_t STARTING_HAND_SIZE  = 13;
	inline constexpr uint8_t INVALID_SEAT     = 0xFF;

	// Rule flags live in one place and travel inside GameState, so a state is
	// self-describing and a determinized rollout cannot drift from the table it
	// was sampled under.
	//
	// VERIFICATION STATUS: the fields marked UNVERIFIED below are best guesses
	// at Mahjong Soul's behaviour and have not been checked against the client.
	// The cheapest way to settle all of them is the M4 log corpus: replay real
	// Mahjong Soul hands and assert the scorer reproduces the recorded point
	// deltas. Until then, treat them as knobs, not as facts.
	struct Rules
	{
		uint8_t numPlayers     = 4;
		bool    allowChi       = true;   // sanma has no chi
		bool    nukidora       = false;  // sanma: North is pulled as a bonus tile
		uint8_t akaPerSuit[3]  = { 1, 1, 1 };
		int32_t startingPoints = 25000;
		int32_t returnPoints   = 30000;

		bool kuitan        = true;
		bool doubleRon     = true;   // triple ron still aborts the hand
		bool nagashiMangan = true;
		bool agariYame     = true;   // dealer may end the game at all-last
		bool kuikae        = false;  // may a caller immediately discard what it just claimed

		bool kiriageMangan  = false; // UNVERIFIED
		bool tsumoLoss      = false; // UNVERIFIED - sanma only; Tenhou applies it, Mahjong Soul is believed not to
		bool ronOnKita      = false; // UNVERIFIED - sanma only; may a pulled North be ronned
		bool renhouIsMangan = false; // UNVERIFIED

		// How many physical copies of a tile kind exist under these rules.
		// Sanma removes MAN_2..MAN_8 entirely.
		constexpr uint8_t CopiesOf( const TileId t ) const
		{
			if ( !IsValidTile( t ) ) return 0;
			return ( numPlayers == 3 && !IsSanmaTile( t ) ) ? static_cast<uint8_t>( 0 ) : static_cast<uint8_t>( 4 );
		}

		// 136 for four players, 108 for three ( 27 kinds x 4 ).
		constexpr uint16_t TotalTiles() const
		{
			return ( numPlayers == 3 ) ? static_cast<uint16_t>( 108 ) : static_cast<uint16_t>( 136 );
		}

		// Tiles drawable before the hand runs out: everything but the dead wall
		// and the opening deal.
		constexpr uint16_t LiveWallTiles() const
		{
			return static_cast<uint16_t>( TotalTiles() - DEAD_WALL_TILES - numPlayers * STARTING_HAND_SIZE );
		}
	};

	constexpr Rules MahjongSoul4P()
	{
		Rules r;
		r.numPlayers     = 4;
		r.allowChi       = true;
		r.nukidora       = false;
		r.akaPerSuit[0]  = 1;
		r.akaPerSuit[1]  = 1;
		r.akaPerSuit[2]  = 1;
		r.startingPoints = 25000;
		r.returnPoints   = 30000;
		return r;
	}

	constexpr Rules MahjongSoul3P()
	{
		Rules r;
		r.numPlayers     = 3;
		r.allowChi       = false;
		r.nukidora       = true;
		r.akaPerSuit[0]  = 0; // no 5m exists in sanma
		r.akaPerSuit[1]  = 1; // UNVERIFIED count
		r.akaPerSuit[2]  = 1; // UNVERIFIED count
		r.startingPoints = 35000;
		r.returnPoints   = 40000;
		return r;
	}

} // namespace LRMahjong::Model
