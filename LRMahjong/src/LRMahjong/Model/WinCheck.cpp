#include "WinCheck.h"

namespace LRMahjong::Model
{
	namespace
	{
		uint8_t TotalOf( const Counts34 &counts )
		{
			uint16_t total = 0;
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) total = static_cast<uint16_t>( total + counts[t] );
			return static_cast<uint8_t>( total );
		}

		// Strips `setsNeeded` runs and triplets out of the histogram. Works on
		// a scratch copy and always restores what it borrowed, so the caller's
		// counts are untouched on the way back up.
		bool DecomposeSets( Counts34 &counts, const int setsNeeded )
		{
			TileId first = 0;
			while ( first < TILE_KIND_COUNT && counts[first] == 0 ) ++first;

			if ( setsNeeded == 0 )
			{
				// Everything must have been consumed.
				return first == TILE_KIND_COUNT;
			}

			if ( first == TILE_KIND_COUNT ) return false;

			// Anchoring on the lowest remaining tile makes the search
			// exhaustive without needing to try every starting point.
			if ( counts[first] >= 3 )
			{
				counts[first] = static_cast<uint8_t>( counts[first] - 3 );
				const bool ok = DecomposeSets( counts, setsNeeded - 1 );
				counts[first] = static_cast<uint8_t>( counts[first] + 3 );
				if ( ok ) return true;
			}

			// RankOf <= 7 keeps the run inside one suit.
			if ( IsSuited( first ) && RankOf( first ) <= 7 &&
				counts[first + 1] > 0 && counts[first + 2] > 0 )
			{
				--counts[first];
				--counts[first + 1];
				--counts[first + 2];
				const bool ok = DecomposeSets( counts, setsNeeded - 1 );
				++counts[first];
				++counts[first + 1];
				++counts[first + 2];
				if ( ok ) return true;
			}

			return false;
		}
	}

	bool IsSevenPairs( const Counts34 &concealed )
	{
		if ( TotalOf( concealed ) != 14 ) return false;

		uint8_t pairs = 0;
		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
		{
			if ( concealed[t] == 0 ) continue;

			// Four of a kind is two pairs to a scorer but not to this hand:
			// seven *distinct* pairs are required.
			if ( concealed[t] != 2 ) return false;
			++pairs;
		}

		return pairs == 7;
	}

	bool IsThirteenOrphans( const Counts34 &concealed )
	{
		if ( TotalOf( concealed ) != 14 ) return false;

		uint8_t kinds = 0;
		bool    hasPair = false;

		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
		{
			if ( concealed[t] == 0 ) continue;
			if ( !IsTerminalOrHonor( t ) ) return false;

			if ( concealed[t] == 2 )
			{
				if ( hasPair ) return false;
				hasPair = true;
			}
			else if ( concealed[t] != 1 )
			{
				return false;
			}

			++kinds;
		}

		return kinds == 13 && hasPair;
	}

	bool IsWinningHand( const Counts34 &concealed, const uint8_t meldCount )
	{
		if ( meldCount > 4 ) return false;

		const uint8_t expected = static_cast<uint8_t>( 14 - 3 * meldCount );
		if ( TotalOf( concealed ) != expected ) return false;

		if ( meldCount == 0 )
		{
			if ( IsSevenPairs( concealed ) )      return true;
			if ( IsThirteenOrphans( concealed ) ) return true;
		}

		const int setsNeeded = 4 - meldCount;

		for ( TileId pair = 0; pair < TILE_KIND_COUNT; ++pair )
		{
			if ( concealed[pair] < 2 ) continue;

			Counts34 work = concealed;
			work[pair] = static_cast<uint8_t>( work[pair] - 2 );

			if ( DecomposeSets( work, setsNeeded ) ) return true;
		}

		return false;
	}

	uint64_t WaitingTiles( const Counts34 &concealed, const uint8_t meldCount )
	{
		const uint8_t expected = static_cast<uint8_t>( 13 - 3 * meldCount );
		if ( TotalOf( concealed ) != expected ) return 0;

		uint64_t waits = 0;

		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
		{
			// A fifth copy cannot exist, so it can never be the winning tile.
			if ( concealed[t] >= 4 ) continue;

			Counts34 work = concealed;
			++work[t];

			if ( IsWinningHand( work, meldCount ) ) waits |= ( 1ULL << t );
		}

		return waits;
	}

	bool IsTenpai( const Counts34 &concealed, const uint8_t meldCount )
	{
		return WaitingTiles( concealed, meldCount ) != 0;
	}

} // namespace LRMahjong::Model
