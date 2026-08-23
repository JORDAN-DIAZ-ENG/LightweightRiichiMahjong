#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "Tile.h"

namespace LRMahjong::Model
{
	using TenhouString = std::string;

	// The concealed part of a hand, held as a 34-slot histogram. Melds live on
	// Player, not here.
	//
	// Trivially copyable by design: a Hand is copied once per player on every
	// rollout, so it carries no heap storage and no virtuals.
	class Hand
	{
	public:
		LRM_API void Clear();

		// Returns false if the hand already holds four of that kind, if the
		// tile is invalid, or if that red five is already present.
		LRM_API bool Add( TileId tile, bool aka = false );

		// Returns false if the tile is not in the hand. Removing the last copy
		// of a five also clears its red flag when aka is set.
		LRM_API bool Remove( TileId tile, bool aka = false );

		uint8_t Count( const TileId tile ) const { return IsValidTile( tile ) ? _counts[tile] : static_cast<uint8_t>( 0 ); }
		bool    Contains( const TileId tile ) const { return Count( tile ) > 0; }

		uint8_t TotalTiles() const { return _total; }
		const Counts34 &Counts() const { return _counts; }
		AkaMask Aka() const { return _aka; }

		// "123m456p11z". Red fives are written as 0.
		LRM_API TenhouString ToTenhouString() const;

		// Human readable, space separated: "1m 2m 3m E E".
		LRM_API std::string ToString() const;

		// Returns false on a malformed string or more than 14 tiles.
		LRM_API static bool FromTenhouString( std::string_view text, Hand &outHand );

	private:
		Counts34 _counts{};
		AkaMask  _aka   = AKA_NONE;
		uint8_t  _total = 0;
	};

} // namespace LRMahjong::Model
