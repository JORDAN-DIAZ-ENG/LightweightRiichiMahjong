#include "Tile.h"

namespace LRMahjong::Model
{
	namespace
	{
		// Indexed by ( id - 27 ).
		constexpr const char *HONOR_NAMES[7] = { "E", "S", "W", "N", "WD", "GD", "RD" };
		constexpr char SUIT_CHARS[3] = { 'm', 'p', 's' };
	}

	std::string TileToString( const TileId t )
	{
		if ( IsSuited( t ) )
		{
			return std::string{ static_cast<char>( '0' + RankOf( t ) ), SUIT_CHARS[t / 9] };
		}
		if ( IsHonor( t ) )
		{
			return std::string{ HONOR_NAMES[t - 27] };
		}
		return "?";
	}

	std::string TileToTenhouString( const TileId t )
	{
		if ( IsSuited( t ) )
		{
			return std::string{ static_cast<char>( '0' + RankOf( t ) ), SUIT_CHARS[t / 9] };
		}
		if ( IsHonor( t ) )
		{
			// Honour digits are 1-7 in canonical order, hence ( id - 26 ).
			return std::string{ static_cast<char>( '0' + ( t - 26 ) ), 'z' };
		}
		return "?";
	}

	TileId TileFromTenhouString( const std::string_view text )
	{
		if ( text.size() != 2 ) return INVALID_TILE;

		const char digit = text[0];
		const char suit  = text[1];
		if ( digit < '0' || digit > '9' ) return INVALID_TILE;

		const int rank = digit - '0';

		if ( suit == 'z' )
		{
			if ( rank < 1 || rank > 7 ) return INVALID_TILE;
			return static_cast<TileId>( 26 + rank );
		}

		int suitIndex = -1;
		if ( suit == 'm' ) suitIndex = 0;
		else if ( suit == 'p' ) suitIndex = 1;
		else if ( suit == 's' ) suitIndex = 2;
		else return INVALID_TILE;

		// 0 denotes a red five; it maps onto the ordinary five.
		const int effectiveRank = ( rank == 0 ) ? 5 : rank;
		return static_cast<TileId>( suitIndex * 9 + ( effectiveRank - 1 ) );
	}

	char SuitToTenhouChar( const Suit suit )
	{
		switch ( suit )
		{
		case Suit::MANZU: return 'm';
		case Suit::PINZU: return 'p';
		case Suit::SOUZU: return 's';
		case Suit::HONOR: return 'z';
		default:          return '?';
		}
	}

	std::string CountsToTenhouString( const Counts34 &counts, const AkaMask aka )
	{
		std::string result;

		// Four groups: manzu, pinzu, souzu, honours.
		for ( int group = 0; group < 4; ++group )
		{
			const TileId first = static_cast<TileId>( group * 9 );
			const TileId last  = ( group == 3 ) ? TILE_KIND_COUNT : static_cast<TileId>( first + 9 );

			const size_t lengthBefore = result.size();

			for ( TileId t = first; t < last; ++t )
			{
				uint8_t remaining = counts[t];
				if ( remaining == 0 ) continue;

				// A red five is written as 0 and emitted first.
				const AkaMask bit = AkaBitFor( t );
				if ( bit != AKA_NONE && ( aka & bit ) != 0 )
				{
					result += '0';
					--remaining;
				}

				const char digit = static_cast<char>( '0' + ( IsHonor( t ) ? ( t - 26 ) : RankOf( t ) ) );
				result.append( remaining, digit );
			}

			if ( result.size() != lengthBefore )
			{
				result += ( group == 3 ) ? 'z' : SUIT_CHARS[group];
			}
		}

		return result;
	}

	bool CountsFromTenhouString( const std::string_view text, Counts34 &outCounts, AkaMask &outAka )
	{
		Counts34 counts{};
		AkaMask  aka = AKA_NONE;

		// Digits accumulate until a suit character tells us which suit they were.
		constexpr size_t MAX_PENDING = 32;
		int    pending[MAX_PENDING];
		size_t pendingCount = 0;

		for ( const char c : text )
		{
			if ( c >= '0' && c <= '9' )
			{
				if ( pendingCount >= MAX_PENDING ) return false;
				pending[pendingCount++] = c - '0';
				continue;
			}

			int suitIndex = -1;
			if ( c == 'm' ) suitIndex = 0;
			else if ( c == 'p' ) suitIndex = 1;
			else if ( c == 's' ) suitIndex = 2;
			else if ( c == 'z' ) suitIndex = 3;
			else return false;

			if ( pendingCount == 0 ) return false; // suit character with no digits

			for ( size_t i = 0; i < pendingCount; ++i )
			{
				const int rank = pending[i];
				TileId tile;

				if ( suitIndex == 3 )
				{
					if ( rank < 1 || rank > 7 ) return false;
					tile = static_cast<TileId>( 26 + rank );
				}
				else if ( rank == 0 )
				{
					tile = static_cast<TileId>( suitIndex * 9 + 4 ); // red five
					const AkaMask bit = AkaBitFor( tile );
					if ( ( aka & bit ) != 0 ) return false;          // two of the same red five
					aka |= bit;
				}
				else
				{
					tile = static_cast<TileId>( suitIndex * 9 + ( rank - 1 ) );
				}

				if ( counts[tile] >= 4 ) return false; // fifth copy
				++counts[tile];
			}

			pendingCount = 0;
		}

		if ( pendingCount != 0 ) return false; // trailing digits with no suit

		outCounts = counts;
		outAka    = aka;
		return true;
	}

	bool AlwaysReturnTrue()
	{
		return true;
	}

} // namespace LRMahjong::Model
