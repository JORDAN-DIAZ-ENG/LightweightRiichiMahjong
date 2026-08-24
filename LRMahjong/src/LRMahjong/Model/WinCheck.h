#pragma once

#include <cstdint>

#include "Tile.h"

namespace LRMahjong::Model
{
	// Complete-hand detection and wait computation.
	//
	// This is the correctness-first reference implementation: a plain
	// recursive decomposition, no tables. M2 needs it because furiten, ron and
	// the tenpai half of riichi legality cannot be expressed without it.
	//
	// M3 adds shanten, ukeire and a table-driven fast path, and keeps this
	// version as the oracle to fuzz the fast one against. Nothing here is on a
	// hot path yet, so it is written to be obviously right rather than quick.
	//
	// `concealed` is the hand histogram only; melded sets are counted by
	// `meldCount`, so the concealed part must supply ( 4 - meldCount ) sets
	// plus the pair. Seven pairs and thirteen orphans are only considered for
	// a fully concealed hand.

	// True when the histogram is a complete hand. Note this is a *shape* test:
	// whether the hand also holds a yaku is a scoring question and belongs to
	// M4.
	LRM_API bool IsWinningHand( const Counts34 &concealed, uint8_t meldCount );

	// Every tile that would complete the hand, as a mask over bits 0..33.
	// Returns 0 when the hand is not tenpai.
	LRM_API uint64_t WaitingTiles( const Counts34 &concealed, uint8_t meldCount );

	LRM_API bool IsTenpai( const Counts34 &concealed, uint8_t meldCount );

	// Seven distinct pairs. Concealed hands only.
	LRM_API bool IsSevenPairs( const Counts34 &concealed );

	// Thirteen orphans, with one of them duplicated. Concealed hands only.
	LRM_API bool IsThirteenOrphans( const Counts34 &concealed );

} // namespace LRMahjong::Model
