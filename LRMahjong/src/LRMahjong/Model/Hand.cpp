#include "hand.h"

using namespace LRMahjong::Model;

#include <algorithm>
#include "../RNG.h"
#include <iostream>

Hand::Hand()
{
	generateRandomHand();
}

Hand::Hand( const TenhouString &tenhouString )
{
}

HandArray Hand::GetHand( const bool getSorted )
{
	if ( getSorted )
	{
		Sort();
		return _sorted;
	}
	else
	{
		return _unsorted;
	}
}

void LRMahjong::Model::Hand::PrintHand()
{
	for ( const auto &tile : _unsorted )
	{
		std::cout << tile.PrintTile() << " ";
	}
}

inline char SuitToTenhouChar( Suit suit )
{
	switch ( suit )
	{
	case Suit::MANZU: return 'm';
	case Suit::PINZU: return 'p';
	case Suit::SOUZU: return 's';
	case Suit::HONOR: return 'z';
	default: return '?';
	}
}

std::string Hand::ToTenhouString()
{
	std::string result;

	Suit prevSuit = Suit::UNDEFINED;

	Sort();
	for ( const auto &tile : _sorted )
	{
		Suit currentSuit = tile.suit;
		if ( currentSuit != prevSuit && prevSuit != Suit::UNDEFINED )
		{
			result += SuitToTenhouChar( prevSuit );
		}

		result += tile.ToTenhouDigit();
		prevSuit = currentSuit;
	}

	if ( prevSuit != Suit::UNDEFINED )
	{
		result += SuitToTenhouChar( prevSuit );
	}

	return result;
}


void Hand::generateRandomHand()
{
	for ( Tile& tile : _unsorted )
	{
		int randomTile = RNG::Int( 0, static_cast<int>( RiichiMahjongTile::TILE_COUNT ) - 1 );
		tile.type = static_cast< RiichiMahjongTile >( randomTile );
		tile = Tile( tile.type );
	}
}

void Hand::Sort()
{
	_sorted = _unsorted;
	std::sort( _sorted.begin(), _sorted.end() );
}