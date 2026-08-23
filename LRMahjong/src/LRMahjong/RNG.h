#pragma once

#include <cstdint>
#include <iterator>

namespace LRMahjong
{
	// xoshiro256** with a splitmix64 seeder.
	//
	// Deliberately an instance, not a singleton, and never seeded from
	// std::random_device by default. Two properties matter for this engine:
	//
	//   - Reproducibility. The same seed must always replay the same game, or
	//     no result from a rollout is debuggable.
	//   - Thread independence. Parallel rollouts each own an Rng, so there is
	//     no shared state to contend on and no cross-thread interleaving to
	//     make a run unrepeatable.
	class Rng
	{
	public:
		constexpr explicit Rng( const uint64_t seed = 0x9E3779B97F4A7C15ULL ) noexcept
		{
			Seed( seed );
		}

		constexpr void Seed( uint64_t seed ) noexcept
		{
			// splitmix64 expands a single word into the four-word state.
			for ( uint64_t &word : _state )
			{
				seed += 0x9E3779B97F4A7C15ULL;
				uint64_t z = seed;
				z = ( z ^ ( z >> 30 ) ) * 0xBF58476D1CE4E5B9ULL;
				z = ( z ^ ( z >> 27 ) ) * 0x94D049BB133111EBULL;
				word = z ^ ( z >> 31 );
			}
		}

		constexpr uint64_t NextU64() noexcept
		{
			const uint64_t result = Rotate( _state[1] * 5, 7 ) * 9;
			const uint64_t t = _state[1] << 17;

			_state[2] ^= _state[0];
			_state[3] ^= _state[1];
			_state[1] ^= _state[2];
			_state[0] ^= _state[3];
			_state[2] ^= t;
			_state[3] = Rotate( _state[3], 45 );

			return result;
		}

		constexpr uint32_t NextU32() noexcept { return static_cast<uint32_t>( NextU64() >> 32 ); }

		// Unbiased value in [0, bound). Lemire's multiply-shift with rejection.
		constexpr uint32_t Below( const uint32_t bound ) noexcept
		{
			if ( bound == 0 ) return 0;

			uint32_t value = NextU32();
			uint64_t product = static_cast<uint64_t>( value ) * bound;
			uint32_t low = static_cast<uint32_t>( product );

			if ( low < bound )
			{
				const uint32_t threshold = ( 0u - bound ) % bound;
				while ( low < threshold )
				{
					value = NextU32();
					product = static_cast<uint64_t>( value ) * bound;
					low = static_cast<uint32_t>( product );
				}
			}

			return static_cast<uint32_t>( product >> 32 );
		}

		// Inclusive range, matching the old RNG::Int contract.
		constexpr int Int( const int min, const int max ) noexcept
		{
			if ( max <= min ) return min;
			const uint32_t span = static_cast<uint32_t>( max - min ) + 1u;
			return min + static_cast<int>( Below( span ) );
		}

		// Half-open [0, 1).
		double Double() noexcept
		{
			return static_cast<double>( NextU64() >> 11 ) * 0x1.0p-53;
		}

		// Fisher-Yates. Used to build the wall.
		template <typename Iterator>
		constexpr void Shuffle( Iterator first, Iterator last ) noexcept
		{
			const auto count = last - first;
			for ( auto i = count - 1; i > 0; --i )
			{
				const auto j = static_cast<decltype( i )>( Below( static_cast<uint32_t>( i ) + 1u ) );
				if ( i != j )
				{
					const auto temp = first[i];
					first[i] = first[j];
					first[j] = temp;
				}
			}
		}

		// Save and restore, so a rollout can branch from a known point.
		constexpr void GetState( uint64_t ( &out )[4] ) const noexcept
		{
			for ( int i = 0; i < 4; ++i ) out[i] = _state[i];
		}

		constexpr void SetState( const uint64_t ( &in )[4] ) noexcept
		{
			for ( int i = 0; i < 4; ++i ) _state[i] = in[i];
		}

	private:
		static constexpr uint64_t Rotate( const uint64_t x, const int k ) noexcept
		{
			return ( x << k ) | ( x >> ( 64 - k ) );
		}

		uint64_t _state[4]{};
	};

} // namespace LRMahjong
