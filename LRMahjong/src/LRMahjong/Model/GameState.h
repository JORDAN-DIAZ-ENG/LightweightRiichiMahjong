#pragma once

#include <cstdint>
#include <type_traits>

#include "Action.h"
#include "Player.h"
#include "Rules.h"
#include "Tile.h"

namespace LRMahjong::Model
{
	inline constexpr uint16_t MAX_WALL_TILES = 136;

	// The engine only ever comes to rest in DISCARD, CALL or HAND_OVER. DEAL
	// and DRAW are transient markers: the engine passes through them inside a
	// single Step, because neither involves a decision.
	enum class Phase : uint8_t
	{
		DEAL,      // hand not yet dealt
		DRAW,      // transient: a tile is being handed to the current player
		DISCARD,   // current player holds a drawn tile and must act
		CALL,      // a discard is on the table, reactors may claim it
		HAND_OVER, // the hand has ended
	};

	enum class HandOutcome : uint8_t
	{
		NONE = 0,
		TSUMO,
		RON,
		EXHAUSTIVE_DRAW,    // ryuukyoku: the live wall ran out
		ABORT_KYUUSHU,      // nine distinct terminals and honours
		ABORT_FOUR_KAN,
		ABORT_FOUR_RIICHI,
		ABORT_FOUR_WINDS,
		ABORT_TRIPLE_RON,
	};

	// How a hand finished. Deliberately carries no points: turning this into a
	// score is M4's job, and keeping them apart means the state machine can be
	// tested without a scorer.
	struct HandResult
	{
		HandOutcome  outcome     = HandOutcome::NONE;
		uint8_t      winner      = INVALID_SEAT;       // on a multiple ron, the seat nearest the discarder
		uint8_t      loser       = INVALID_SEAT;       // the discarder, on a ron
		TileInstance winningTile = INVALID_INSTANCE;

		// Mahjong Soul pays every claimant on a double ron, so a single winner
		// field is not enough. Bit per seat; M4 iterates this to score.
		uint8_t winnerMask = 0;

		bool haitei  = false; // won on the last tile of the live wall
		bool houtei  = false; // won on the final discard
		bool rinshan = false; // won on a kan replacement tile
		bool chankan = false; // robbed an added kan
	};

	// Full perfect-information state of one hand.
	//
	// This is the type rollouts run on, so it is a flat aggregate: fixed-size
	// arrays, no heap, no virtuals, trivially copyable. Cloning a state is a
	// memcpy of well under a kilobyte, which is what makes millions of playouts
	// per second reachable.
	//
	// Rules travel inside the state so a state is self-describing and sanma and
	// four-player share one type.
	struct GameState
	{
		Rules rules{};

		// The whole wall in draw order.
		//
		//   live wall     [liveWallHead, liveWallTail)
		//   dead wall     [DeadWallStart(), wallCount)   -- always 14 tiles
		//        + 0 ..  4   dora indicators
		//        + 5 ..  9   ura dora indicators
		//        +10 .. 13   kan and kita replacements
		//
		// This is not the physical layout of a real wall, but it is equivalent
		// and the indices stay absolute. Each replacement draw pulls the live
		// wall's last tile into the dead wall by decrementing liveWallTail,
		// which is what shortens the hand by one draw per kan.
		TileInstance wall[MAX_WALL_TILES]{};
		uint8_t wallCount     = 0; // 108 or 136
		uint8_t liveWallHead  = 0; // index of the next live draw
		uint8_t liveWallTail  = 0; // one past the last drawable live tile
		uint8_t deadWallDraws = 0; // replacement tiles taken, at most 4

		uint8_t doraIndicators = 0; // how many of the 5 indicators are face up

		uint8_t roundWind     = 0; // 0 = east round
		uint8_t dealer        = 0;
		uint8_t currentPlayer = 0;
		uint8_t honba         = 0;
		uint8_t riichiSticks  = 0;

		Phase phase = Phase::DEAL;

		TileInstance lastDiscard   = INVALID_INSTANCE;
		uint8_t      lastDiscarder = INVALID_SEAT;

		// Seats that have not yet responded to lastDiscard, as a bitmask. The
		// discard resolves when this reaches zero.
		uint8_t pendingCallers = 0;

		// What each seat answered with. Responses are collected rather than
		// applied on arrival, because ron outranks pon and kan, which outrank
		// chi, and that ordering cannot be honoured until every seat has
		// spoken.
		Action pendingResponse[MAX_PLAYERS]{};

		// A added kan can be robbed. While this is set the call window accepts
		// only ron and pass, and a ron taken here is chankan.
		bool awaitingChankan = false;

		// Kuikae: the tiles the current player may not discard, having just
		// claimed one. Mask over bits 0..33, cleared on the next discard.
		uint64_t forbiddenDiscards = 0;

		// A called kan flips its indicator only after the caller discards; a
		// concealed kan flips immediately.
		bool pendingDoraFlip = false;

		// Cleared the moment anyone calls. Gates the first-go-around rules:
		// double riichi, kyuushu kyuuhai and suufon renda.
		bool firstGoAround = true;

		bool drewFromDeadWall = false; // the tile in hand came from a kan replacement

		Player players[MAX_PLAYERS]{};

		HandResult result{};

		// Sets up seats, points and wind assignment for a fresh hand. Filling
		// and dealing the wall is Engine::StartHand.
		LRM_API void Reset( const Rules &newRules, uint8_t newDealer );

		// ---- wall -----------------------------------------------------

		uint8_t DeadWallStart() const { return static_cast<uint8_t>( wallCount - DEAD_WALL_TILES ); }

		bool LiveWallEmpty() const { return liveWallHead >= liveWallTail; }

		uint8_t LiveWallRemaining() const
		{
			return LiveWallEmpty() ? static_cast<uint8_t>( 0 ) : static_cast<uint8_t>( liveWallTail - liveWallHead );
		}

		// Indicator i, counting from the first one turned face up. Returns
		// INVALID_INSTANCE when i is beyond what has been revealed.
		LRM_API TileInstance DoraIndicator( uint8_t i ) const;
		LRM_API TileInstance UraIndicator( uint8_t i ) const;

		// Takes the next live tile, or INVALID_INSTANCE if the wall is spent.
		LRM_API TileInstance DrawLive();

		// Takes a kan or kita replacement and shortens the live wall by one.
		// Returns INVALID_INSTANCE once all four have gone.
		LRM_API TileInstance DrawReplacement();

		LRM_API void RevealDoraIndicator();

		uint8_t PlayerCount() const { return rules.numPlayers; }

		uint8_t NextSeat( const uint8_t seat ) const
		{
			return static_cast<uint8_t>( ( seat + 1 ) % rules.numPlayers );
		}

		// Total kans declared across every seat, for the four-kan abort.
		LRM_API uint8_t TotalKans() const;

		// Every tile the given seat can see: its own hand, all melds, all
		// discards, pulled Norths and the face-up dora indicators. The
		// complement of this is the pool the observation layer samples from.
		LRM_API Counts34 VisibleTo( uint8_t viewer ) const;

		// The complement of VisibleTo: copies of each kind the viewer has not
		// accounted for, and so could still draw or be dealt into. This is both
		// the weight behind ukeire and the pool the belief layer samples from.
		LRM_API Counts34 UnseenFrom( uint8_t viewer ) const;
	};

	// A rollout copies this type; if either assertion ever fails, the copy has
	// stopped being a memcpy and the search budget has quietly changed.
	static_assert( std::is_trivially_copyable_v<GameState>, "GameState must stay memcpy-able for rollouts" );
	static_assert( sizeof( GameState ) <= 1024, "GameState has outgrown its 1 KB budget" );

} // namespace LRMahjong::Model
