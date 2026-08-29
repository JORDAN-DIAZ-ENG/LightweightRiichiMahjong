#include "Shanten.h"

#include <atomic>

namespace LRMahjong::Model
{
	namespace
	{
		constexpr int MAX_BLOCKS = 5; // four sets and a pair

		// Turns a decomposition into a shanten count. Blocks past the fifth are
		// worthless, and a hand holding five blocks with no pair among them has
		// to spend a tile turning one into the pair.
		int8_t ShantenFromBlocks( const int melds, int partials, const bool hasPair )
		{
			if ( melds + partials > MAX_BLOCKS ) partials = MAX_BLOCKS - melds;

			int shanten = 8 - 2 * melds - partials;
			if ( melds + partials == MAX_BLOCKS && !hasPair ) ++shanten;

			return static_cast<int8_t>( shanten );
		}

		// ---------------------------------------------------------------
		// Fast path: profile each suit on its own, then recombine.
		// ---------------------------------------------------------------

		// best[melds][hasPair] is the most partials that group can yield while
		// giving up exactly that many melds. -1 marks a combination the group
		// cannot reach.
		struct GroupProfile
		{
			int8_t best[5][2];

			void Reset()
			{
				for ( int m = 0; m < 5; ++m )
				{
					best[m][0] = -1;
					best[m][1] = -1;
				}
			}

			void Record( int melds, int partials, const bool pair )
			{
				if ( melds > 4 )        melds = 4;
				if ( partials > MAX_BLOCKS ) partials = MAX_BLOCKS;

				const int8_t p = static_cast<int8_t>( partials );

				if ( p > best[melds][0] ) best[melds][0] = p;
				if ( pair && p > best[melds][1] ) best[melds][1] = p;
			}
		};

		// One numbered suit: triplets, runs, pairs, and the two shapes of
		// incomplete run.
		void ExploreSuited( uint8_t *c, const int from, const int melds, const int partials,
			const bool pair, GroupProfile &out )
		{
			int r = from;
			while ( r < 9 && c[r] == 0 ) ++r;

			if ( r == 9 )
			{
				out.Record( melds, partials, pair );
				return;
			}

			const bool room = ( melds + partials ) < MAX_BLOCKS;

			if ( room && c[r] >= 3 )
			{
				c[r] = static_cast<uint8_t>( c[r] - 3 );
				ExploreSuited( c, r, melds + 1, partials, pair, out );
				c[r] = static_cast<uint8_t>( c[r] + 3 );
			}

			if ( room && r <= 6 && c[r + 1] > 0 && c[r + 2] > 0 )
			{
				--c[r]; --c[r + 1]; --c[r + 2];
				ExploreSuited( c, r, melds + 1, partials, pair, out );
				++c[r]; ++c[r + 1]; ++c[r + 2];
			}

			if ( room && c[r] >= 2 )
			{
				c[r] = static_cast<uint8_t>( c[r] - 2 );
				ExploreSuited( c, r, melds, partials + 1, true, out );
				c[r] = static_cast<uint8_t>( c[r] + 2 );
			}

			// Adjacent pair, so a ryanmen or a penchan.
			if ( room && r <= 7 && c[r + 1] > 0 )
			{
				--c[r]; --c[r + 1];
				ExploreSuited( c, r, melds, partials + 1, pair, out );
				++c[r]; ++c[r + 1];
			}

			// Gap of one, so a kanchan.
			if ( room && r <= 6 && c[r + 2] > 0 )
			{
				--c[r]; --c[r + 2];
				ExploreSuited( c, r, melds, partials + 1, pair, out );
				++c[r]; ++c[r + 2];
			}

			// Or the tile simply does not take part.
			--c[r];
			ExploreSuited( c, r, melds, partials, pair, out );
			++c[r];
		}

		// Honours have no runs, so only triplets and pairs.
		void ExploreHonors( uint8_t *c, const int from, const int melds, const int partials,
			const bool pair, GroupProfile &out )
		{
			int r = from;
			while ( r < 7 && c[r] == 0 ) ++r;

			if ( r == 7 )
			{
				out.Record( melds, partials, pair );
				return;
			}

			const bool room = ( melds + partials ) < MAX_BLOCKS;

			if ( room && c[r] >= 3 )
			{
				c[r] = static_cast<uint8_t>( c[r] - 3 );
				ExploreHonors( c, r, melds + 1, partials, pair, out );
				c[r] = static_cast<uint8_t>( c[r] + 3 );
			}

			if ( room && c[r] >= 2 )
			{
				c[r] = static_cast<uint8_t>( c[r] - 2 );
				ExploreHonors( c, r, melds, partials + 1, true, out );
				c[r] = static_cast<uint8_t>( c[r] + 2 );
			}

			--c[r];
			ExploreHonors( c, r, melds, partials, pair, out );
			++c[r];
		}

		// ---------------------------------------------------------------
		// Profile cache.
		//
		// A group's profile depends only on its own nine (or seven) counts, so
		// it is worth computing once. The counts form a base-5 number, which
		// indexes the table directly.
		//
		// The tables are filled lazily rather than up front. Both live in .bss,
		// so the pages are only committed as they are touched: a run that never
		// sees an exotic suit shape never pays for it. Together they reserve
		// about 8 MB of address space and in practice touch a small fraction.
		//
		// Two threads racing to fill the same slot both compute the same value,
		// so the relaxed atomic is only there to keep the write well defined,
		// not to order anything.
		// ---------------------------------------------------------------

		constexpr uint32_t SUIT_TABLE_SIZE  = 1953125; // 5^9
		constexpr uint32_t HONOR_TABLE_SIZE = 78125;   // 5^7
		constexpr uint32_t PROFILE_COMPUTED = 1u << 31;
		constexpr uint8_t  PROFILE_NONE     = 7;       // stands in for -1

		std::atomic<uint32_t> g_suitTable[SUIT_TABLE_SIZE];
		std::atomic<uint32_t> g_honorTable[HONOR_TABLE_SIZE];

		uint32_t Base5Index( const uint8_t *c, const int ranks )
		{
			uint32_t index = 0;
			for ( int i = ranks - 1; i >= 0; --i ) index = index * 5 + c[i];
			return index;
		}

		uint32_t PackProfile( const GroupProfile &p )
		{
			uint32_t packed = PROFILE_COMPUTED;
			int shift = 0;

			for ( int m = 0; m < 5; ++m )
			for ( int q = 0; q < 2; ++q )
			{
				const uint32_t value = ( p.best[m][q] < 0 )
					? PROFILE_NONE
					: static_cast<uint32_t>( p.best[m][q] );

				packed |= ( value << shift );
				shift += 3;
			}

			return packed;
		}

		GroupProfile UnpackProfile( const uint32_t packed )
		{
			GroupProfile p;
			int shift = 0;

			for ( int m = 0; m < 5; ++m )
			for ( int q = 0; q < 2; ++q )
			{
				const uint32_t value = ( packed >> shift ) & 0x7u;
				p.best[m][q] = ( value == PROFILE_NONE ) ? static_cast<int8_t>( -1 )
					: static_cast<int8_t>( value );
				shift += 3;
			}

			return p;
		}

		GroupProfile SuitedProfile( const Counts34 &concealed, const int suit )
		{
			uint8_t work[9];
			bool cacheable = true;

			for ( int r = 0; r < 9; ++r )
			{
				work[r] = concealed[suit * 9 + r];
				if ( work[r] > 4 ) cacheable = false;
			}

			// A count above four cannot be indexed, and cannot arise from a
			// legal hand either; compute it directly rather than trusting it.
			if ( !cacheable )
			{
				GroupProfile p;
				p.Reset();
				ExploreSuited( work, 0, 0, 0, false, p );
				return p;
			}

			const uint32_t index  = Base5Index( work, 9 );
			const uint32_t cached = g_suitTable[index].load( std::memory_order_relaxed );
			if ( ( cached & PROFILE_COMPUTED ) != 0 ) return UnpackProfile( cached );

			GroupProfile p;
			p.Reset();
			ExploreSuited( work, 0, 0, 0, false, p );

			g_suitTable[index].store( PackProfile( p ), std::memory_order_relaxed );
			return p;
		}

		GroupProfile HonorProfile( const Counts34 &concealed )
		{
			uint8_t work[7];
			bool cacheable = true;

			for ( int r = 0; r < 7; ++r )
			{
				work[r] = concealed[27 + r];
				if ( work[r] > 4 ) cacheable = false;
			}

			if ( !cacheable )
			{
				GroupProfile p;
				p.Reset();
				ExploreHonors( work, 0, 0, 0, false, p );
				return p;
			}

			const uint32_t index  = Base5Index( work, 7 );
			const uint32_t cached = g_honorTable[index].load( std::memory_order_relaxed );
			if ( ( cached & PROFILE_COMPUTED ) != 0 ) return UnpackProfile( cached );

			GroupProfile p;
			p.Reset();
			ExploreHonors( work, 0, 0, 0, false, p );

			g_honorTable[index].store( PackProfile( p ), std::memory_order_relaxed );
			return p;
		}

		// Walks the four group profiles, accumulating every reachable
		// (melds, partials, pair) and keeping the best shanten found.
		// Only the *most* partials matter for a given meld count and pair
		// status, so the state is [melds][hasPair] rather than a full
		// reachability set over partial counts too.
		//
		// Taking fewer partials is never an improvement: dropping one raises
		// the count by one, and the only case where it could help is dodging
		// the missing-pair penalty at five blocks, which lands on exactly the
		// same number. Since the pair flag is tracked separately, both readings
		// are already covered.
		int8_t CombineProfiles( const GroupProfile groups[4], const uint8_t meldCount )
		{
			int8_t dp[5][2];
			for ( int m = 0; m < 5; ++m ) { dp[m][0] = -1; dp[m][1] = -1; }
			dp[meldCount][0] = 0;

			for ( int g = 0; g < 4; ++g )
			{
				int8_t next[5][2];
				for ( int m = 0; m < 5; ++m ) { next[m][0] = -1; next[m][1] = -1; }

				for ( int m = 0; m <= 4; ++m )
				for ( int q = 0; q < 2; ++q )
				{
					if ( dp[m][q] < 0 ) continue;

					for ( int gm = 0; gm + m <= 4; ++gm )
					{
						// A group that can field gm melds can always field
						// fewer, so the profile is dense from zero upward and
						// the first gap is the last entry.
						if ( groups[g].best[gm][0] < 0 && groups[g].best[gm][1] < 0 ) break;

						for ( int gq = 0; gq < 2; ++gq )
						{
							const int8_t cap = groups[g].best[gm][gq];
							if ( cap < 0 ) continue;

							const int melds = m + gm;

							int partials = dp[m][q] + cap;
							if ( melds + partials > MAX_BLOCKS ) partials = MAX_BLOCKS - melds;

							const int nq = q | gq;
							if ( partials > next[melds][nq] ) next[melds][nq] = static_cast<int8_t>( partials );
						}
					}
				}

				for ( int m = 0; m < 5; ++m ) { dp[m][0] = next[m][0]; dp[m][1] = next[m][1]; }
			}

			int8_t best = SHANTEN_MAX;

			for ( int m = 0; m <= 4; ++m )
			for ( int q = 0; q < 2; ++q )
			{
				if ( dp[m][q] < 0 ) continue;

				const int8_t s = ShantenFromBlocks( m, dp[m][q], q != 0 );
				if ( s < best ) best = s;
			}

			return best;
		}

		// ---------------------------------------------------------------
		// Reference path: one flat recursion over all 34 tiles.
		// ---------------------------------------------------------------

		void ExploreAll( Counts34 &c, const TileId from, const int melds, const int partials,
			const bool pair, const uint8_t meldCount, int8_t &best )
		{
			TileId t = from;
			while ( t < TILE_KIND_COUNT && c[t] == 0 ) ++t;

			if ( t == TILE_KIND_COUNT )
			{
				const int8_t s = ShantenFromBlocks( melds + meldCount, partials, pair );
				if ( s < best ) best = s;
				return;
			}

			const bool room = ( melds + meldCount + partials ) < MAX_BLOCKS;
			const bool runnable = IsSuited( t ) && RankOf( t ) <= 7;

			if ( room && c[t] >= 3 )
			{
				c[t] = static_cast<uint8_t>( c[t] - 3 );
				ExploreAll( c, t, melds + 1, partials, pair, meldCount, best );
				c[t] = static_cast<uint8_t>( c[t] + 3 );
			}

			if ( room && runnable && c[t + 1] > 0 && c[t + 2] > 0 )
			{
				--c[t]; --c[t + 1]; --c[t + 2];
				ExploreAll( c, t, melds + 1, partials, pair, meldCount, best );
				++c[t]; ++c[t + 1]; ++c[t + 2];
			}

			if ( room && c[t] >= 2 )
			{
				c[t] = static_cast<uint8_t>( c[t] - 2 );
				ExploreAll( c, t, melds, partials + 1, true, meldCount, best );
				c[t] = static_cast<uint8_t>( c[t] + 2 );
			}

			if ( room && IsSuited( t ) && RankOf( t ) <= 8 && c[t + 1] > 0 )
			{
				--c[t]; --c[t + 1];
				ExploreAll( c, t, melds, partials + 1, pair, meldCount, best );
				++c[t]; ++c[t + 1];
			}

			if ( room && runnable && c[t + 2] > 0 )
			{
				--c[t]; --c[t + 2];
				ExploreAll( c, t, melds, partials + 1, pair, meldCount, best );
				++c[t]; ++c[t + 2];
			}

			--c[t];
			ExploreAll( c, t, melds, partials, pair, meldCount, best );
			++c[t];
		}
	}

	int8_t StandardShanten( const Counts34 &concealed, const uint8_t meldCount )
	{
		if ( meldCount > 4 ) return SHANTEN_MAX;

		const GroupProfile groups[4] = {
			SuitedProfile( concealed, 0 ),
			SuitedProfile( concealed, 1 ),
			SuitedProfile( concealed, 2 ),
			HonorProfile( concealed ),
		};

		return CombineProfiles( groups, meldCount );
	}

	int8_t SevenPairsShanten( const Counts34 &concealed )
	{
		int pairs = 0;
		int kinds = 0;

		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
		{
			if ( concealed[t] == 0 ) continue;
			++kinds;
			if ( concealed[t] >= 2 ) ++pairs;
		}

		// Fewer than seven distinct kinds means some of the duplicates have to
		// be traded out as well, one exchange each.
		int shanten = 6 - pairs;
		if ( kinds < 7 ) shanten += 7 - kinds;

		return static_cast<int8_t>( shanten );
	}

	int8_t ThirteenOrphansShanten( const Counts34 &concealed )
	{
		int kinds = 0;
		bool hasPair = false;

		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
		{
			if ( !IsTerminalOrHonor( t ) || concealed[t] == 0 ) continue;

			++kinds;
			if ( concealed[t] >= 2 ) hasPair = true;
		}

		return static_cast<int8_t>( 13 - kinds - ( hasPair ? 1 : 0 ) );
	}

	int8_t Shanten( const Counts34 &concealed, const uint8_t meldCount )
	{
		int8_t best = StandardShanten( concealed, meldCount );

		// Neither special form survives a call.
		if ( meldCount != 0 ) return best;

		// Both special forms are answered from one pass over the histogram
		// rather than a scan each.
		int  pairs       = 0;
		int  kinds       = 0;
		int  orphanKinds = 0;
		bool orphanPair  = false;

		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
		{
			const uint8_t n = concealed[t];
			if ( n == 0 ) continue;

			++kinds;
			if ( n >= 2 ) ++pairs;

			if ( IsTerminalOrHonor( t ) )
			{
				++orphanKinds;
				if ( n >= 2 ) orphanPair = true;
			}
		}

		int sevenPairs = 6 - pairs;
		if ( kinds < 7 ) sevenPairs += 7 - kinds;
		if ( sevenPairs < best ) best = static_cast<int8_t>( sevenPairs );

		const int orphans = 13 - orphanKinds - ( orphanPair ? 1 : 0 );
		if ( orphans < best ) best = static_cast<int8_t>( orphans );

		return best;
	}

	int8_t ShantenReference( const Counts34 &concealed, const uint8_t meldCount )
	{
		if ( meldCount > 4 ) return SHANTEN_MAX;

		Counts34 work = concealed;
		int8_t best = SHANTEN_MAX;

		ExploreAll( work, 0, 0, 0, false, meldCount, best );

		if ( meldCount == 0 )
		{
			const int8_t pairs = SevenPairsShanten( concealed );
			if ( pairs < best ) best = pairs;

			const int8_t orphans = ThirteenOrphansShanten( concealed );
			if ( orphans < best ) best = orphans;
		}

		return best;
	}

	uint64_t Ukeire( const Counts34 &concealed, const uint8_t meldCount )
	{
		const int8_t current = Shanten( concealed, meldCount );
		if ( current == SHANTEN_COMPLETE ) return 0;

		uint64_t mask = 0;
		Counts34 work = concealed;

		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
		{
			// A fifth copy does not exist, so it cannot be drawn.
			if ( work[t] >= 4 ) continue;

			++work[t];
			if ( Shanten( work, meldCount ) < current ) mask |= ( 1ULL << t );
			--work[t];
		}

		return mask;
	}

	UkeireResult UkeireAgainst( const Counts34 &concealed, const uint8_t meldCount,
		const Counts34 &remaining )
	{
		UkeireResult result;

		result.shanten = Shanten( concealed, meldCount );
		result.tiles   = Ukeire( concealed, meldCount );

		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
		{
			if ( ( result.tiles & ( 1ULL << t ) ) != 0 )
			{
				result.copies = static_cast<uint16_t>( result.copies + remaining[t] );
			}
		}

		return result;
	}

} // namespace LRMahjong::Model
