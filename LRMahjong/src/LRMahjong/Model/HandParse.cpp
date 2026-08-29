#include "HandParse.h"

#include "WinCheck.h"

namespace LRMahjong::Model
{
	namespace
	{
		struct ParseState
		{
			Decomposition      working;
			DecompositionList *out = nullptr;
			int                setsNeeded = 0;
		};

		// Strips runs and triplets out of the histogram, recording each one, and
		// hands the completed reading to the list when nothing is left.
		void ExtractSets( Counts34 &counts, ParseState &state, const int taken )
		{
			if ( taken == state.setsNeeded )
			{
				for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
				{
					if ( counts[t] != 0 ) return; // tiles left over, not a reading
				}

				state.out->Add( state.working );
				return;
			}

			TileId first = 0;
			while ( first < TILE_KIND_COUNT && counts[first] == 0 ) ++first;
			if ( first == TILE_KIND_COUNT ) return;

			// Anchoring on the lowest remaining tile keeps the search exhaustive
			// without revisiting the same reading from a different direction.
			if ( counts[first] >= 3 )
			{
				counts[first] = static_cast<uint8_t>( counts[first] - 3 );

				HandSet &set = state.working.sets[state.working.setCount++];
				set = HandSet{ SetType::TRIPLET, first, true, false };

				ExtractSets( counts, state, taken + 1 );

				--state.working.setCount;
				counts[first] = static_cast<uint8_t>( counts[first] + 3 );
			}

			if ( IsSuited( first ) && RankOf( first ) <= 7 &&
				counts[first + 1] > 0 && counts[first + 2] > 0 )
			{
				--counts[first]; --counts[first + 1]; --counts[first + 2];

				HandSet &set = state.working.sets[state.working.setCount++];
				set = HandSet{ SetType::RUN, first, true, false };

				ExtractSets( counts, state, taken + 1 );

				--state.working.setCount;
				++counts[first]; ++counts[first + 1]; ++counts[first + 2];
			}
		}
	}

	uint8_t Decompose( const Counts34 &concealed, const Meld *melds, const uint8_t meldCount,
		DecompositionList &out )
	{
		out.count = 0;

		if ( meldCount > 4 ) return 0;

		// ---- the two special forms, closed hands only ------------------
		if ( meldCount == 0 )
		{
			if ( IsSevenPairs( concealed ) )
			{
				Decomposition d;
				d.sevenPairs = true;

				// Seven pairs has no four-sets reading, so the pairs are
				// recorded for the yaku that still care about the tiles.
				for ( TileId t = 0; t < TILE_KIND_COUNT && d.setCount < 5; ++t )
				{
					if ( concealed[t] == 2 ) d.sets[d.setCount++] = HandSet{ SetType::PAIR, t, true, false };
				}

				out.Add( d );
			}

			if ( IsThirteenOrphans( concealed ) )
			{
				Decomposition d;
				d.thirteenOrphans = true;
				out.Add( d );
			}
		}

		// ---- four sets and a pair --------------------------------------
		const int setsNeeded = 4 - meldCount;

		for ( TileId pair = 0; pair < TILE_KIND_COUNT; ++pair )
		{
			if ( concealed[pair] < 2 ) continue;

			Counts34 work = concealed;
			work[pair] = static_cast<uint8_t>( work[pair] - 2 );

			ParseState state;
			state.out        = &out;
			state.setsNeeded = setsNeeded;

			// Called sets are fixed and go in first, so every reading carries
			// the whole hand and not just its concealed part.
			for ( uint8_t i = 0; i < meldCount; ++i )
			{
				const Meld &meld = melds[i];

				HandSet set;
				set.tile     = meld.base;
				set.fromCall = true;

				switch ( meld.type )
				{
				case MeldType::CHI:
					set.type = SetType::RUN;
					set.concealed = false;
					break;
				case MeldType::PON:
					set.type = SetType::TRIPLET;
					set.concealed = false;
					break;
				case MeldType::ANKAN:
					set.type = SetType::KAN;
					set.concealed = true;
					break;
				case MeldType::MINKAN:
				case MeldType::SHOUMINKAN:
					set.type = SetType::KAN;
					set.concealed = false;
					break;
				default:
					continue;
				}

				state.working.sets[state.working.setCount++] = set;
			}

			state.working.sets[state.working.setCount++] = HandSet{ SetType::PAIR, pair, true, false };

			ExtractSets( work, state, 0 );
		}

		return out.count;
	}

} // namespace LRMahjong::Model
