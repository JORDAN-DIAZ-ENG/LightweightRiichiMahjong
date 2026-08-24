#pragma once

#include <cstdint>

#include "Hand.h"
#include "Meld.h"
#include "Rules.h"

namespace LRMahjong::Model
{
	// Upper bound on discards by a single player in one hand. Four-player has
	// 70 live draws in total; calls add discards without a draw, so 32 leaves
	// generous headroom.
	inline constexpr uint8_t MAX_DISCARDS = 32;
	inline constexpr uint8_t MAX_MELDS    = 4;
	inline constexpr uint8_t INVALID_INDEX = 0xFF;

	using DiscardFlags = uint8_t;
	inline constexpr DiscardFlags DISCARD_NONE       = 0;
	inline constexpr DiscardFlags DISCARD_TSUMOGIRI  = 1 << 0; // drawn and immediately discarded
	inline constexpr DiscardFlags DISCARD_RIICHI     = 1 << 1; // the riichi declaration tile
	inline constexpr DiscardFlags DISCARD_CALLED     = 1 << 2; // taken by another player

	// One seat's full state. A plain struct with fixed-size storage so that
	// GameState stays trivially copyable and a rollout is a memcpy.
	//
	// Discards are kept as an ordered list rather than a histogram because both
	// furiten and the observation layer need the order they were played in.
	struct Player
	{
		Hand    hand{};
		Meld    melds[MAX_MELDS]{};
		uint8_t meldCount = 0;

		TileInstance discards[MAX_DISCARDS]{};
		DiscardFlags discardFlags[MAX_DISCARDS]{};
		uint8_t      discardCount = 0;

		int32_t points   = 0;
		uint8_t seatWind = 0;                        // 0 = east, offset from RiichiMahjongTile::EAST
		uint8_t nukiCount = 0;                       // sanma: North tiles pulled as bonus dora

		TileInstance drawn = INVALID_INSTANCE;       // the tile drawn this turn, for tsumogiri detection

		// True between receiving a tile ( by draw or by call ) and discarding it.
		// A call does not set `drawn`, because the called tile goes to the meld
		// rather than the hand, but it does leave the seat owing a discard.
		bool awaitingDiscard = false;

		bool riichiDeclared = false;
		bool doubleRiichi   = false;
		bool ippatsu        = false;
		uint8_t riichiDiscardIndex = INVALID_INDEX; // index into discards[] of the declaration tile

		bool furitenPermanent = false;               // passed on a winning tile while in riichi
		bool furitenTemporary = false;               // cleared on this player's next draw

		// Concealed tiles this seat should be holding: 13 minus 3 per meld, plus
		// one while a discard is owed. Holds for kan and for nuki, because both
		// take a replacement tile.
		constexpr uint8_t ExpectedHandSize() const
		{
			const uint8_t base = static_cast<uint8_t>( STARTING_HAND_SIZE - 3 * meldCount );
			return awaitingDiscard ? static_cast<uint8_t>( base + 1 ) : base;
		}

		constexpr bool IsMenzen() const
		{
			for ( uint8_t i = 0; i < meldCount; ++i )
			{
				if ( !melds[i].IsConcealed() ) return false;
			}
			return true;
		}

		// Every tile this seat has revealed: melds, discards and pulled Norths.
		LRM_API void AddRevealedTo( Counts34 &counts ) const;
	};

} // namespace LRMahjong::Model
