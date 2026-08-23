#pragma once

#include <cstdint>

#include "../RNG.h"
#include "GameState.h"
#include "Meld.h"
#include "Rules.h"
#include "Tile.h"

// ---------------------------------------------------------------------------
// Observation, Belief and determinization ( implemented in M5 ).
//
// The shapes live here from M0 because they constrain GameState. Two GameState
// decisions above exist only to serve this file: discards are an ordered list
// rather than a histogram, and every seat carries an explicit hand size. Both
// would have been cheaper to omit and expensive to retrofit.
//
// Three distinct types, and collapsing any two of them is the failure mode:
//
//   GameState   - full perfect information. Simulation and rollouts run on it.
//   Observation - an append-only log of what the viewer actually saw, with a
//                 confidence per field.
//   Belief      - the constraint set over the GameStates consistent with an
//                 Observation.
//
// Observation is a log rather than a snapshot because uncertainty resolves
// backwards. If a discard was missed on turn 3 but the pond is read cleanly on
// turn 8, the later reading has to be able to fill in the earlier gap; a
// snapshot has already discarded the question. Discard order is also load
// bearing for furiten. The log additionally gives the assistant an incremental
// update path, which matters when frames arrive several times a second.
// ---------------------------------------------------------------------------

namespace LRMahjong::Model
{
	// How much to trust a single observed field. An OCR frontend sets this from
	// its own confidence; a clean log source sets everything to CERTAIN.
	enum class Conf : uint8_t
	{
		UNKNOWN = 0, // not observed at all
		LOW     = 1,
		HIGH    = 2,
		CERTAIN = 3, // observed without doubt, or deduced
	};

	template <typename T>
	struct Observed
	{
		T    value{};
		Conf conf = Conf::UNKNOWN;

		constexpr bool IsKnown() const { return conf == Conf::CERTAIN; }
		constexpr bool IsSeen()  const { return conf != Conf::UNKNOWN; }
	};

	// Identity uncertainty: "a tile is here, but not which one". One bit per
	// kind, so the whole distribution fits in a register.
	struct TileBelief
	{
		uint64_t possible = 0; // bits 0..33

		constexpr bool CanBe( const TileId t ) const { return ( possible & ( 1ULL << t ) ) != 0; }
		constexpr void Allow( const TileId t )       { possible |= ( 1ULL << t ); }
		constexpr void Deny( const TileId t )        { possible &= ~( 1ULL << t ); }
		constexpr bool IsEmpty() const               { return possible == 0; }

		static constexpr TileBelief Any()     { return TileBelief{ ( 1ULL << TILE_KIND_COUNT ) - 1ULL }; }
		static constexpr TileBelief Exactly( const TileId t ) { return TileBelief{ 1ULL << t }; }
	};

	// Structural uncertainty: "I do not know *whether* that happened". This
	// cannot be folded into TileBelief, which is why events carry their own
	// confidence rather than relying on the tile mask alone.
	enum class EventType : uint8_t
	{
		NONE = 0,
		HAND_START,
		DRAW,
		DISCARD,
		CHI,
		PON,
		KAN,
		KITA,        // sanma: North pulled as a bonus tile
		RIICHI,
		DORA_FLIP,
		TSUMO,
		RON,
		DRAW_ABORT,
		HAND_END,
	};

	struct Event
	{
		EventType    type  = EventType::NONE;
		uint8_t      actor = INVALID_SEAT;
		TileInstance tile  = INVALID_INSTANCE; // the tile involved, if identified
		TileBelief   tileBelief{};             // used when tile is not identified
		Conf         conf  = Conf::UNKNOWN;    // confidence that this event happened at all
		uint8_t      target = INVALID_SEAT;    // discarder, for a call or a ron
	};

	inline constexpr uint16_t MAX_EVENTS = 512;

	// One hand as seen from one seat.
	struct Observation
	{
		Rules    rules{};
		uint8_t  viewer = INVALID_SEAT;
		Event    events[MAX_EVENTS]{};
		uint16_t eventCount = 0;

		bool Append( const Event &e );
	};

	// What is known about one opponent's concealed hand. The size is always
	// known even when the contents are not, because it follows from the meld
	// count and the phase.
	struct HandBelief
	{
		uint8_t  size = STARTING_HAND_SIZE;
		Counts34 known{};      // tiles confirmed to be in this hand
		uint64_t excluded = 0; // kinds this seat provably cannot hold
	};

	// The constraint set. Determinization samples the unseen pool into the
	// opponents' remaining slots and the wall, respecting `excluded`.
	//
	// `excluded` is where reads live: a riichi player is furiten against their
	// own discards, and tiles they declined to call constrain them too. M5a
	// fills it from the hard constraints only; the prior-weighted opponent
	// model in M5c is what turns it from a filter into a distribution.
	struct Belief
	{
		Rules      rules{};
		uint8_t    viewer = INVALID_SEAT;
		Counts34   unseen{};              // total copies minus everything visible
		HandBelief hands[MAX_PLAYERS]{};
		uint8_t    liveWallRemaining = 0;

		// Rebuilt from scratch.
		void BuildFrom( const Observation &obs );

		// Incremental: the assistant path, which cannot afford a full rebuild
		// per frame.
		void Apply( const Event &e );
	};

	enum class ReconcileResult : uint8_t
	{
		CONSISTENT, // the observation describes a reachable state
		REPAIRED,   // low-confidence observations were dropped to make it reachable
		IMPOSSIBLE, // no repair within policy produced a reachable state
	};

	// Inconsistent input is the normal case with an OCR frontend, not an edge
	// case: a fifth copy of a tile, a hand of twelve, discard counts that do not
	// match the turn number. A live assistant cannot answer that by crashing.
	enum class RepairPolicy : uint8_t
	{
		STRICT,             // never drop anything; report IMPOSSIBLE instead
		DROP_LEAST_CONFIDENT, // drop ascending by confidence until consistent
	};

	ReconcileResult Reconcile( Observation &obs, RepairPolicy policy );

	// Samples one concrete world consistent with the belief. This is the bridge
	// from "what the player can see" to "something the engine can roll out".
	bool Determinize( const Belief &belief, Rng &rng, GameState &outState );

} // namespace LRMahjong::Model
