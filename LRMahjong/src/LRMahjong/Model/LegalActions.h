#pragma once

#include <cstdint>

#include "Action.h"
#include "GameState.h"
#include "Tile.h"

namespace LRMahjong::Model
{
	// Enumerates exactly what a seat may legally do right now.
	//
	// This is the single source of truth for legality. Engine::Step performs
	// the same checks when it applies an action, and the two are held together
	// by a test asserting that Step accepts every generated action and rejects
	// everything else. Generation and application drifting apart is the
	// classic way a mahjong engine develops rules bugs that only appear deep
	// in a search.
	//
	// Returns the number of actions written. A seat with nothing to do gets 0.
	LRM_API uint8_t LegalActions( const GameState &state, uint8_t seat, ActionList &out );

	// Furiten in all three forms:
	//
	//   - own discards: any tile in the wait set sitting in the seat's own
	//     pond, which blocks ron until the wait changes
	//   - temporary: declined a winning discard since the last draw, clearing
	//     on the next draw
	//   - permanent: declined a winning discard while in riichi, and the wait
	//     can no longer change, so it never clears
	//
	// A furiten hand may still win by tsumo.
	LRM_API bool IsFuriten( const GameState &state, uint8_t seat );

	// The tiles a seat is waiting on, as a mask over bits 0..33. Empty when
	// the hand is not tenpai.
	LRM_API uint64_t SeatWaits( const GameState &state, uint8_t seat );

	// Tiles the caller may not discard after claiming one, per the kuikae
	// rule: the claimed tile itself, and for a chi taken at either end of the
	// run, the tile at the far end.
	LRM_API uint64_t KuikaeMask( MeldType type, TileId base, TileId called );

} // namespace LRMahjong::Model
