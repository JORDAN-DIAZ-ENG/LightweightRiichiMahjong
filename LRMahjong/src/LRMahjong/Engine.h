#pragma once

#include <cstdint>

#include "Core.h"
#include "RNG.h"
#include "Model/Action.h"
#include "Model/GameState.h"
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

		// Called once every reactor has passed on the live discard.
		void ResolveDiscard();

		void EndHand( Model::HandOutcome outcome,
			uint8_t winner = Model::INVALID_SEAT,
			uint8_t loser  = Model::INVALID_SEAT );

		// Shared bookkeeping for chi, pon and daiminkan.
		void CompleteCall( uint8_t seat );

		uint8_t OtherSeatsMask( uint8_t seat ) const;

		bool FourKanAbort() const;
		bool FourRiichiAbort() const;
		bool FourWindsAbort() const;

		bool HasNineTerminalKinds( uint8_t seat ) const;

		Model::Rules     _rules;
		Model::GameState _state{};
		Rng              _rng;
		uint64_t         _seed = 0;
	};

} // namespace LRMahjong
