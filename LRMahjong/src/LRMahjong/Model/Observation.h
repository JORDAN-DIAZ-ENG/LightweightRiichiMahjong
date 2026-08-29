#pragma once

#include <cstdint>

#include "../RNG.h"
#include "GameState.h"
#include "Meld.h"
#include "Rules.h"
#include "Tile.h"

// ---------------------------------------------------------------------------
// Observation, Belief and determinization.
//
// Three distinct types, and collapsing any two of them is the failure mode:
//
//   GameState   - full perfect information. Simulation and rollouts run on it.
//   Observation - an append-only log of what the viewer actually saw, with a
//                 confidence per event.
//   Belief      - the constraint set over the GameStates consistent with an
//                 Observation.
//
// Observation is a log rather than a snapshot because uncertainty resolves
// backwards. If a discard was missed on turn 3 but the pond is read cleanly on
// turn 8, the later reading has to be able to fill in the earlier gap; a
// snapshot has already discarded the question. Discard order is also load
// bearing for furiten. The log additionally gives the assistant an incremental
// update path, which matters when frames arrive several times a second.
//
// Two shapes changed from the M0 sketch, once there was an implementation to
// answer to:
//
//   - HandBelief::excluded, a bitmask of kinds a seat cannot hold, became
//     maxHeld, an upper bound per kind. The strongest thing an observer can
//     actually prove about an opponent is "declined to pon that tile, so holds
//     at most one copy", which is a count and not a yes or no.
//
//   - Belief gained publicState. Determinization has to produce a whole
//     GameState, and everything the viewer can already see belongs in one
//     place rather than being rebuilt at sampling time.
// ---------------------------------------------------------------------------

namespace LRMahjong::Model
{
	// How much to trust a single observed event. An OCR frontend sets this from
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

		static constexpr TileBelief Any() { return TileBelief{ ( 1ULL << TILE_KIND_COUNT ) - 1ULL }; }
		static constexpr TileBelief Exactly( const TileId t ) { return TileBelief{ 1ULL << t }; }
	};

	// Structural uncertainty: "I do not know *whether* that happened". This
	// cannot be folded into TileBelief, which is why events carry their own
	// confidence rather than relying on the tile mask alone.
	enum class EventType : uint8_t
	{
		NONE = 0,
		DRAW,
		DISCARD,
		CALL,        // chi, pon or a called kan; meldType says which
		ANKAN,
		KITA,        // sanma: North pulled as a bonus tile
		RIICHI,
		DORA_FLIP,
		TSUMO,
		RON,
		ABORT,
	};

	struct Event
	{
		EventType type  = EventType::NONE;
		uint8_t   actor = INVALID_SEAT;

		// The tile involved when it was identified. A draw the viewer could not
		// see leaves this invalid and leans on tileBelief instead.
		TileInstance tile = INVALID_INSTANCE;
		TileBelief   tileBelief{};

		Conf     conf     = Conf::UNKNOWN; // that the event happened at all
		uint8_t  target   = INVALID_SEAT;  // the discarder, for a call or a ron
		TileId   base     = INVALID_TILE;  // lowest tile of a chi, or the repeated tile
		MeldType meldType = MeldType::NONE;
		AkaMask  meldAka  = AKA_NONE;

		LRM_API static Event Draw( uint8_t seat, TileInstance t, Conf c = Conf::CERTAIN );
		LRM_API static Event Discard( uint8_t seat, TileInstance t, Conf c = Conf::CERTAIN );
		LRM_API static Event Call( uint8_t seat, MeldType type, TileId base, TileInstance called,
			uint8_t from, AkaMask aka, Conf c = Conf::CERTAIN );
		LRM_API static Event Ankan( uint8_t seat, TileId base, AkaMask aka, Conf c = Conf::CERTAIN );
		LRM_API static Event Kita( uint8_t seat, Conf c = Conf::CERTAIN );
		LRM_API static Event Riichi( uint8_t seat, Conf c = Conf::CERTAIN );
		LRM_API static Event DoraFlip( TileInstance indicator, Conf c = Conf::CERTAIN );
	};

	inline constexpr uint16_t MAX_EVENTS = 512;

	// One hand as seen from one seat: the conditions it started under, and
	// everything that happened since.
	struct Observation
	{
		Rules   rules{};
		uint8_t viewer       = INVALID_SEAT;
		uint8_t dealer       = 0;
		uint8_t roundWind    = 0;
		uint8_t honba        = 0;
		uint8_t riichiSticks = 0;

		// What the viewer was dealt. Opponents' opening hands are never seen.
		Counts34 startingHand{};
		AkaMask  startingAka = AKA_NONE;

		Event    events[MAX_EVENTS]{};
		uint16_t eventCount = 0;

		LRM_API bool Append( const Event &e );
		LRM_API void Clear();
	};

	// What is known about one seat's concealed hand. The size is always known
	// even when the contents are not, because it follows from the meld count
	// and whose turn it is.
	struct HandBelief
	{
		uint8_t  size = 0;
		Counts34 known{};   // tiles confirmed to be in this hand
		Counts34 maxHeld{}; // upper bound per kind, from the pool and from reads
	};

	// The constraint set. Determinization samples the unseen pool into the
	// opponents' remaining slots and the wall, respecting maxHeld.
	//
	// maxHeld is where reads live. M5 fills it from hard constraints only: what
	// the pool can still supply, and what a seat proved it did not hold by
	// declining a call it could have made. Turning that from a filter into a
	// distribution is the opponent model, and is not built here.
	struct Belief
	{
		Rules      rules{};
		uint8_t    viewer = INVALID_SEAT;
		Counts34   unseen{};
		HandBelief hands[MAX_PLAYERS]{};

		// Everything the viewer can see, with the opponents' hands left empty.
		GameState publicState{};

		bool consistent = false;

		// Rebuilt from scratch.
		LRM_API void BuildFrom( const Observation &obs );

		// Incremental: the assistant path, which cannot afford a full rebuild
		// per frame. BuildFrom is this in a loop, so there is one implementation
		// of what an event means.
		LRM_API void Apply( const Event &e );

		LRM_API void Reset( const Observation &obs );
	};

	enum class ReconcileResult : uint8_t
	{
		CONSISTENT, // the observation describes a reachable state
		REPAIRED,   // low-confidence events were dropped to make it reachable
		IMPOSSIBLE, // no repair within policy produced a reachable state
	};

	// Inconsistent input is the normal case with an OCR frontend, not an edge
	// case: a fifth copy of a tile, a hand of twelve, discard counts that do not
	// match the turn number. A live assistant cannot answer that by crashing.
	enum class RepairPolicy : uint8_t
	{
		STRICT,               // never drop anything; report IMPOSSIBLE instead
		DROP_LEAST_CONFIDENT, // drop ascending by confidence until consistent
	};

	LRM_API ReconcileResult Reconcile( Observation &obs, RepairPolicy policy );

	struct DeterminizeOptions
	{
		// A seat that declared riichi was tenpai when it did and cannot have
		// changed its wait since, so a sampled hand that is not tenpai is known
		// to be wrong. Costs attempts, and buys worlds that are actually
		// possible.
		bool respectRiichiTenpai = true;

		uint16_t maxAttempts = 64;
	};

	// Samples one concrete world consistent with the belief. This is the bridge
	// from "what the player can see" to "something the engine can roll out".
	LRM_API bool Determinize( const Belief &belief, Rng &rng, GameState &outState,
		const DeterminizeOptions &options = DeterminizeOptions{} );

} // namespace LRMahjong::Model
