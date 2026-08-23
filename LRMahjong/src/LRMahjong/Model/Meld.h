#pragma once

#include <cstdint>

#include "Tile.h"

namespace LRMahjong::Model
{
	enum class MeldType : uint8_t
	{
		NONE = 0,
		CHI,
		PON,
		ANKAN,      // concealed kan
		MINKAN,     // called kan ( daiminkan )
		SHOUMINKAN, // added kan, upgraded from a pon
	};

	// Which seat the tile was taken from, relative to the caller.
	enum class CalledFrom : uint8_t
	{
		SELF   = 0, // concealed kan
		RIGHT  = 1, // shimocha
		ACROSS = 2, // toimen
		LEFT   = 3, // kamicha
	};

	// Five plain bytes rather than the packed 16-bit Tenhou encoding. A meld is
	// read far more often than it is copied, and four melds per player costs
	// 20 bytes of a ~640 byte state, so legibility wins. A converter to and from
	// the Tenhou encoding belongs with the rest of the log import ( M7 ).
	struct Meld
	{
		MeldType     type   = MeldType::NONE;
		TileId       base   = INVALID_TILE;       // lowest tile of a chi, or the repeated tile
		TileInstance called = INVALID_INSTANCE;   // the tile taken from another player
		CalledFrom   from   = CalledFrom::SELF;
		AkaMask      aka    = AKA_NONE;           // red fives contained in this meld

		constexpr bool IsKan() const
		{
			return type == MeldType::ANKAN || type == MeldType::MINKAN || type == MeldType::SHOUMINKAN;
		}

		// A concealed kan still counts as a closed hand for menzen purposes.
		constexpr bool IsConcealed() const { return type == MeldType::ANKAN; }

		constexpr uint8_t TileCount() const { return IsKan() ? static_cast<uint8_t>( 4 ) : static_cast<uint8_t>( 3 ); }

		// Adds this meld's tiles into a 34-slot histogram.
		constexpr void AddTo( Counts34 &counts ) const
		{
			switch ( type )
			{
			case MeldType::CHI:
				++counts[base];
				++counts[base + 1];
				++counts[base + 2];
				break;
			case MeldType::PON:
				counts[base] = static_cast<uint8_t>( counts[base] + 3 );
				break;
			case MeldType::ANKAN:
			case MeldType::MINKAN:
			case MeldType::SHOUMINKAN:
				counts[base] = static_cast<uint8_t>( counts[base] + 4 );
				break;
			case MeldType::NONE:
			default:
				break;
			}
		}
	};

} // namespace LRMahjong::Model
