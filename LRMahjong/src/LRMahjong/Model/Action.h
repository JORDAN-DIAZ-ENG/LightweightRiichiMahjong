#pragma once

#include <cstdint>

#include "Rules.h"
#include "Tile.h"

namespace LRMahjong::Model
{
	// Every choice a seat can make. The engine is driven entirely by these:
	// there is no internal loop and no callback into a player object, because
	// search needs to apply an action to a copy of the state and a live
	// assistant needs to feed in actions it merely observed.
	//
	// Drawing is not an action. A seat whose turn arrives has no choice about
	// receiving a tile, so the engine performs the draw itself and comes to
	// rest in DISCARD. Making it an explicit step would double the step count
	// of a rollout to represent a decision nobody makes.
	enum class ActionType : uint8_t
	{
		NONE = 0,

		// From Phase::DISCARD, by the current player.
		DISCARD,
		RIICHI,      // declare riichi and discard `tile` in one action
		ANKAN,       // concealed kan of `base`
		SHOUMINKAN,  // add `base` to an existing pon
		KITA,        // sanma: pull a North as a bonus tile
		TSUMO,
		KYUUSHU,     // nine distinct terminals and honours, abortive draw

		// From Phase::CALL, by a seat other than the discarder.
		CHI,         // claim the discard into the run starting at `base`
		PON,
		DAIMINKAN,
		RON,
		PASS,
	};

	struct Action
	{
		ActionType   type  = ActionType::NONE;
		uint8_t      actor = INVALID_SEAT;

		// The tile leaving the hand, for DISCARD and RIICHI. Carries its own
		// red-five bit.
		TileInstance tile = INVALID_INSTANCE;

		// Lowest tile of a chi, or the repeated tile of a pon or kan.
		TileId base = INVALID_TILE;

		// Which red fives this action moves out of the hand and into the meld.
		// The caller chooses, because melding the red copy or keeping it is a
		// real decision whenever the hand holds more copies than the meld needs.
		AkaMask meldAka = AKA_NONE;

		static constexpr Action Discard( const uint8_t seat, const TileInstance t )
		{
			Action a; a.type = ActionType::DISCARD; a.actor = seat; a.tile = t; return a;
		}

		static constexpr Action Riichi( const uint8_t seat, const TileInstance t )
		{
			Action a; a.type = ActionType::RIICHI; a.actor = seat; a.tile = t; return a;
		}

		static constexpr Action Pass( const uint8_t seat )
		{
			Action a; a.type = ActionType::PASS; a.actor = seat; return a;
		}

		static constexpr Action Chi( const uint8_t seat, const TileId runBase, const AkaMask aka = AKA_NONE )
		{
			Action a; a.type = ActionType::CHI; a.actor = seat; a.base = runBase; a.meldAka = aka; return a;
		}

		static constexpr Action Pon( const uint8_t seat, const TileId t, const AkaMask aka = AKA_NONE )
		{
			Action a; a.type = ActionType::PON; a.actor = seat; a.base = t; a.meldAka = aka; return a;
		}

		static constexpr Action Daiminkan( const uint8_t seat, const TileId t, const AkaMask aka = AKA_NONE )
		{
			Action a; a.type = ActionType::DAIMINKAN; a.actor = seat; a.base = t; a.meldAka = aka; return a;
		}

		static constexpr Action Ankan( const uint8_t seat, const TileId t, const AkaMask aka = AKA_NONE )
		{
			Action a; a.type = ActionType::ANKAN; a.actor = seat; a.base = t; a.meldAka = aka; return a;
		}

		static constexpr Action Shouminkan( const uint8_t seat, const TileId t, const AkaMask aka = AKA_NONE )
		{
			Action a; a.type = ActionType::SHOUMINKAN; a.actor = seat; a.base = t; a.meldAka = aka; return a;
		}

		static constexpr Action Kita( const uint8_t seat )
		{
			Action a; a.type = ActionType::KITA; a.actor = seat; return a;
		}

		static constexpr Action Tsumo( const uint8_t seat )
		{
			Action a; a.type = ActionType::TSUMO; a.actor = seat; return a;
		}

		static constexpr Action Ron( const uint8_t seat )
		{
			Action a; a.type = ActionType::RON; a.actor = seat; return a;
		}

		static constexpr Action Kyuushu( const uint8_t seat )
		{
			Action a; a.type = ActionType::KYUUSHU; a.actor = seat; return a;
		}
	};

	// Two actions are the same choice when every field a caller can pick
	// matches. Used to check a submitted action against the generated set.
	constexpr bool operator==( const Action &a, const Action &b )
	{
		return a.type == b.type
			&& a.actor == b.actor
			&& a.tile == b.tile
			&& a.base == b.base
			&& a.meldAka == b.meldAka;
	}

	constexpr bool operator!=( const Action &a, const Action &b ) { return !( a == b ); }

	enum class StepResult : uint8_t
	{
		OK,         // applied, the hand continues
		HAND_ENDED, // applied, and the hand is now over
		ILLEGAL,    // rejected; the state is untouched
	};

	// A generated set of choices. Fixed capacity and no heap, because action
	// generation runs at every node of a search.
	//
	// The widest case is a fourteen tile hand that can also declare riichi:
	// at most fourteen distinct discards plus three red-five variants, doubled
	// for the riichi form, plus kans, kita, tsumo and kyuushu. That stays
	// under fifty.
	struct ActionList
	{
		static constexpr uint8_t CAPACITY = 64;

		Action  actions[CAPACITY]{};
		uint8_t count = 0;

		constexpr void Clear() { count = 0; }

		constexpr bool Add( const Action &a )
		{
			if ( count >= CAPACITY ) return false;
			actions[count++] = a;
			return true;
		}

		constexpr bool Contains( const Action &a ) const
		{
			for ( uint8_t i = 0; i < count; ++i )
			{
				if ( actions[i] == a ) return true;
			}
			return false;
		}

		constexpr bool ContainsType( const ActionType type ) const
		{
			for ( uint8_t i = 0; i < count; ++i )
			{
				if ( actions[i].type == type ) return true;
			}
			return false;
		}

		constexpr const Action *begin() const { return actions; }
		constexpr const Action *end()   const { return actions + count; }

		constexpr const Action &operator[]( const uint8_t i ) const { return actions[i]; }
	};

} // namespace LRMahjong::Model
