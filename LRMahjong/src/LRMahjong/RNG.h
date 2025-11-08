#pragma once
#include <random>
#include <cstdint>

class RNG
{
public:
	// Seed the generator (optional)
	static void Seed( uint32_t seed )
	{
		GetRNGEngine().seed( seed );
	}

	// Returns a random integer in [min, max] inclusive
	static int Int( int min, int max )
	{
		std::uniform_int_distribution<int> dist( min, max );
		return dist( GetRNGEngine() );
	}

	// Returns a random float in [0,1)
	static float Float()
	{
		std::uniform_real_distribution<float> dist( 0.0f, 1.0f );
		return dist( GetRNGEngine() );
	}

private:
	// Private constructor to prevent instances
	RNG() = default;

	// The static RNG engine
	static std::mt19937 &GetRNGEngine()
	{
		static std::mt19937 engine( std::random_device{}( ) ); // seeded once per program
		return engine;
	}
};
