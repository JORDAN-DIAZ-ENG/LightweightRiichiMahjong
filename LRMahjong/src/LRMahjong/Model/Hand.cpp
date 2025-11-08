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

std::string Hand::ToTenhouString()
{
	std::string result;

	Sort();
	for ( const auto &tile : _sorted )
	{
		result += tile.ToTenhouString();
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