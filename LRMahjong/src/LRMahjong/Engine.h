#pragma once

#include <cstdint>

#include "Core.h"
#include "RNG.h"
#include "Model/Action.h"
#include "Model/GameState.h"
#include "Model/LegalActions.h"
#include "Model/Observation.h"
#include "Model/Rules.h"

namespace LRMahjong
{
	// Drives one hand of mahjong.
	//
	// The engine is a step function, not a loop. Every transition is
	// Step( action ), which is what lets a search apply an action to a copied
	// state and what lets an assistant replay actions it only observed. There
	// is no callback into a player object anywhere.
	//
	// Legality lives in one place: Step validates against LegalActions rather
	// than re-deriving the rules, so generation and application cannot drift
	// apart. That drift is the classic source of mahjong engine bugs that only
	// surface deep inside a search.
	//
	// Not exported as a class: only the members that cross the DLL boundary
	// carry LRM_API, so a consumer can inline State() and Random() and reach
	// the hot paths without a thunk.
	class Engine
	{
	public:
		LRM_API explicit Engine( const Model::Rules &rules = Model::MahjongSoul4P(), uint64_t seed = 0 );

		// Clears the state and reseeds. The same seed always replays the same
		// game.
		LRM_API void Reset( uint64_t seed );

		// Builds and shuffles the wall, deals, turns the first dora indicator
		// face up and hands the dealer their fourteenth tile. Leaves the state
		// in Phase::DISCARD.
		LRM_API void StartHand( uint8_t dealer = 0 );

		// Applies one action. On ILLEGAL the state is left untouched, so a
		// caller may probe without cloning first.
		LRM_API Model::StepResult Step( const Model::Action &action );

		const Model::GameState &State() const { return _state; }
		Model::GameState       &State()       { return _state; }

		const Model::Rules      &GetRules() const { return _rules; }
		const Model::HandResult &Result() const   { return _state.result; }

		// Event recording is off by default: a rollout has no use for it, and an
		// Observation is eight kilobytes that would be copied with the engine.
		LRM_API void EnableEventLog( bool enabled );

		const Model::Observation &EventLog() const { return _log; }

		// The log redacted to what one seat could actually have seen: other
		// seats keep their draws as events, but not the tiles.
		LRM_API Model::Observation ViewFor( uint8_t viewer ) const;

		Rng &Random() { return _rng; }

		uint64_t SeedValue() const { return _seed; }

	private:
		void BuildWall();
		void DealHands();

		bool GiveDrawTo( uint8_t seat, bool fromDeadWall );

		Model::StepResult StepDiscardPhase( const Model::Action &action );
		Model::StepResult StepCallPhase( const Model::Action &action );

		Model::StepResult ApplyDiscard( const Model::Action &action );
		Model::StepResult ApplyAnkan( const Model::Action &action );
		Model::StepResult ApplyShouminkan( const Model::Action &action );
		Model::StepResult ApplyKita( const Model::Action &action );
		Model::StepResult ApplyChi( const Model::Action &action );
		Model::StepResult ApplyPon( const Model::Action &action );
		Model::StepResult ApplyDaiminkan( const Model::Action &action );

		// Every reactor has answered: settle them in priority order, ron above
		// pon and kan, pon and kan above chi.
		Model::StepResult ResolveResponses();

		// Nobody claimed the discard, so the turn moves on.
		void ResolveDiscard();

		// A robbed kan window closed with no takers.
		Model::StepResult CompleteChankanPass();

		// Opens the window and answers for any seat whose only choice is to
		// pass, resolving straight away when nobody has a real decision.
		Model::StepResult OpenCallWindow( uint8_t excludingSeat );

		// A seat that declined a tile it could have won on is furiten until its
		// next draw, and for the rest of the hand if it is in riichi.
		void UpdateFuritenOnPass( uint8_t seat );

		void EndHand( Model::HandOutcome outcome,
			uint8_t winner = Model::INVALID_SEAT,
			uint8_t loser  = Model::INVALID_SEAT );

		void EndHandWithRon( uint8_t ronMask );

		// Shared bookkeeping for chi, pon and daiminkan.
		void CompleteCall( uint8_t seat, Model::MeldType type, Model::TileId base );

		uint8_t OtherSeatsMask( uint8_t seat ) const;

		bool FourKanAbort() const;
		bool FourRiichiAbort() const;
		bool FourWindsAbort() const;

		void Record( const Model::Event &e );
		void RevealDora();

		Model::Rules     _rules;
		Model::GameState _state{};

		// Recorded at the deal so ViewFor can hand back the seat's own opening
		// hand, which no event ever carries.
		Model::Counts34 _startingHands[Model::MAX_PLAYERS]{};
		Model::AkaMask  _startingAka[Model::MAX_PLAYERS]{};

		Model::Observation _log{};
		bool               _logging = false;

		Rng              _rng;
		uint64_t         _seed = 0;
	};

} // namespace LRMahjong
