#pragma once

#include <cstdint>

#include "Meld.h"
#include "Tile.h"

namespace LRMahjong::Model
{
	enum class SetType : uint8_t
	{
		RUN,
		TRIPLET,
		KAN,
		PAIR,
	};

	struct HandSet
	{
		SetType type      = SetType::PAIR;
		TileId  tile      = INVALID_TILE; // lowest of a run, or the repeated tile
		bool    concealed = true;         // an ankou scores more than a minkou
		bool    fromCall  = false;        // came from a meld rather than the hand

		constexpr bool IsTripletLike() const { return type == SetType::TRIPLET || type == SetType::KAN; }
	};

	// One reading of a hand as four sets and a pair.
	struct Decomposition
	{
		HandSet sets[5]{};
		uint8_t setCount = 0;

		bool sevenPairs      = false;
		bool thirteenOrphans = false;
	};

	inline constexpr uint8_t MAX_DECOMPOSITIONS = 32;

	struct DecompositionList
	{
		Decomposition items[MAX_DECOMPOSITIONS]{};
		uint8_t       count = 0;

		bool Add( const Decomposition &d )
		{
			if ( count >= MAX_DECOMPOSITIONS ) return false;
			items[count++] = d;
			return true;
		}

		const Decomposition *begin() const { return items; }
		const Decomposition *end()   const { return items + count; }
	};

	// Enumerates every way the hand reads as four sets and a pair, plus the two
	// special forms.
	//
	// A hand does not have one reading, it has several, and both fu and yaku
	// depend on which one you take. 111222333m is three triplets or three runs;
	// the first scores sanankou, the second scores iipeiko twice. The scorer
	// has to try them all and keep the best, so decomposition has to hand back
	// all of them rather than the first that works.
	//
	// Readings may repeat when a hand is symmetric enough; scoring takes a
	// maximum, so duplicates cost a little time and change no answer.
	LRM_API uint8_t Decompose( const Counts34 &concealed, const Meld *melds, uint8_t meldCount,
		DecompositionList &out );

} // namespace LRMahjong::Model
