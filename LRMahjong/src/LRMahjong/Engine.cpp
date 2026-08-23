#include "Engine.h"

namespace LRMahjong
{
	Engine::Engine( const Model::Rules &rules, const uint64_t seed )
		: _rules( rules )
		, _rng( seed )
		, _seed( seed )
	{
		_state.Reset( _rules, 0 );
	}

	void Engine::Reset( const uint64_t seed )
	{
		_seed = seed;
		_rng.Seed( seed );
		_state.Reset( _rules, 0 );
	}

} // namespace LRMahjong
