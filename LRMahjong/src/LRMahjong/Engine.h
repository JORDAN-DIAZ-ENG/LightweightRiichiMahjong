#pragma once

#include <cstdint>

#include "Core.h"
#include "RNG.h"
#include "Model/GameState.h"
#include "Model/Rules.h"

namespace LRMahjong
{
	// Owns one hand's worth of state plus the generator that produced it.
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

		const Model::GameState &State() const { return _state; }
		Model::GameState       &State()       { return _state; }

		const Model::Rules &GetRules() const { return _rules; }

		Rng &Random() { return _rng; }

		uint64_t SeedValue() const { return _seed; }

	private:
		Model::Rules     _rules;
		Model::GameState _state{};
		Rng              _rng;
		uint64_t         _seed = 0;
	};

} // namespace LRMahjong
