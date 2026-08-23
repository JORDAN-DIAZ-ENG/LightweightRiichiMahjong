#include "Hand.h"

#include <numeric>

namespace LRMahjong::Model
{
	void Hand::Clear()
	{
		_counts.fill( 0 );
		_aka   = AKA_NONE;
		_total = 0;
	}

	bool Hand::Add( const TileId tile, const bool aka )
	{
		if ( !IsValidTile( tile ) ) return false;
		if ( _counts[tile] >= 4 )   return false;

		if ( aka )
		{
			const AkaMask bit = AkaBitFor( tile );
			if ( bit == AKA_NONE )     return false; // this kind has no red variant
			if ( ( _aka & bit ) != 0 ) return false; // already holding that red five
			_aka |= bit;
		}

		++_counts[tile];
		++_total;
		return true;
	}

	bool Hand::Remove( const TileId tile, const bool aka )
	{
		if ( !IsValidTile( tile ) ) return false;
		if ( _counts[tile] == 0 )   return false;

		if ( aka )
		{
			const AkaMask bit = AkaBitFor( tile );
			if ( bit == AKA_NONE )     return false;
			if ( ( _aka & bit ) == 0 ) return false; // not holding that red five
			_aka = static_cast<AkaMask>( _aka & ~bit );
		}

		--_counts[tile];
		--_total;
		return true;
	}

	TenhouString Hand::ToTenhouString() const
	{
		return CountsToTenhouString( _counts, _aka );
	}

	std::string Hand::ToString() const
	{
		std::string result;
		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
		{
			for ( uint8_t i = 0; i < _counts[t]; ++i )
			{
				if ( !result.empty() ) result += ' ';
				result += TileToString( t );
			}
		}
		return result;
	}

	bool Hand::FromTenhouString( const std::string_view text, Hand &outHand )
	{
		Counts34 counts{};
		AkaMask  aka = AKA_NONE;

		if ( !CountsFromTenhouString( text, counts, aka ) ) return false;

		const int total = std::accumulate( counts.begin(), counts.end(), 0 );
		if ( total > 14 ) return false;

		outHand._counts = counts;
		outHand._aka    = aka;
		outHand._total  = static_cast<uint8_t>( total );
		return true;
	}

} // namespace LRMahjong::Model
