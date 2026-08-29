#pragma once

#include <cstdint>

#include "Tile.h"

namespace LRMahjong::Model
{
	// Shanten: how many tile exchanges separate a hand from tenpai.
	//
	//   -1  complete
	//    0  tenpai
	//    n  n exchanges away
	//
	// The block formula behind all of this is
	//
	//     shanten = 8 - 2 * melds - partials
	//
	// with the blocks capped at five, and one added back when all five blocks
	// are present but none of them is a pair, since a hand needs its pair and
	// one block has to become it.
	//
	// `concealed` is the hand histogram alone; sets already melded are counted
	// by `meldCount`. Seven pairs and thirteen orphans only apply to a fully
	// concealed hand.
	//
	// Shanten is a measure of *shape* and deliberately ignores the four-copy
	// limit. A hand whose only wait is on a tile it already holds all four of
	// is shanten 0 here, while WaitingTiles reports no winning tile at all.
	// That is the karaten case, and both answers are right: the hand is tenpai
	// for the purposes of riichi and the exhaustive draw, and can never win.
	// Anything that needs winnability rather than shape must ask WaitingTiles.

	inline constexpr int8_t SHANTEN_COMPLETE = -1;

	// Worst possible for a thirteen tile hand: no blocks at all.
	inline constexpr int8_t SHANTEN_MAX = 8;

	// Best of the three hand forms.
	LRM_API int8_t Shanten( const Counts34 &concealed, uint8_t meldCount );

	// The individual forms, exposed because M4 needs to know which one a hand
	// is actually going for.
	LRM_API int8_t StandardShanten( const Counts34 &concealed, uint8_t meldCount );
	LRM_API int8_t SevenPairsShanten( const Counts34 &concealed );
	LRM_API int8_t ThirteenOrphansShanten( const Counts34 &concealed );

	// A second, independent implementation kept purely as a test oracle: one
	// flat recursion over all 34 tiles, where the fast path splits the hand by
	// suit, profiles each group and recombines them. The two disagree only if
	// the grouping or the recombination is wrong, which is where the bugs
	// actually live. Never call this on a hot path.
	LRM_API int8_t ShantenReference( const Counts34 &concealed, uint8_t meldCount );

	// Which tiles would reduce the shanten of this hand, as a mask over bits
	// 0..33. Purely a question of shape: whether any copies are left to draw
	// is answered by UkeireAgainst.
	LRM_API uint64_t Ukeire( const Counts34 &concealed, uint8_t meldCount );

	struct UkeireResult
	{
		uint64_t tiles   = 0; // which kinds improve the hand
		uint16_t copies  = 0; // how many of them are actually still out there
		int8_t   shanten = SHANTEN_MAX;
	};

	// Ukeire weighted by what remains unseen. `remaining[t]` is how many copies
	// of t the viewer has not accounted for; GameState::UnseenFrom builds it.
	//
	// Keeping shape and availability apart matters: a wait on a tile with no
	// copies left is still the hand's shape, and an analysis layer wants to say
	// so rather than pretend the hand is waiting on nothing.
	LRM_API UkeireResult UkeireAgainst( const Counts34 &concealed, uint8_t meldCount,
		const Counts34 &remaining );

} // namespace LRMahjong::Model
