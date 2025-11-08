#include "Tile.h"

#ifdef LRM_DEBUG_API

using namespace LRMahjong::Model;

Tile::Tile( const RiichiMahjongTile tileType ) : type( tileType )
{
	determineSuit();
}

std::string LRMahjong::Model::Tile::PrintTile() const
{
	switch ( type )
	{
			// Man (Characters)
		case RiichiMahjongTile::MAN_1: return "1m";
		case RiichiMahjongTile::MAN_2: return "2m";
		case RiichiMahjongTile::MAN_3: return "3m";
		case RiichiMahjongTile::MAN_4: return "4m";
		case RiichiMahjongTile::MAN_5: return "5m";
		case RiichiMahjongTile::MAN_6: return "6m";
		case RiichiMahjongTile::MAN_7: return "7m";
		case RiichiMahjongTile::MAN_8: return "8m";
		case RiichiMahjongTile::MAN_9: return "9m";

			// Pin (Dots / Circles)
		case RiichiMahjongTile::PIN_1: return "1p";
		case RiichiMahjongTile::PIN_2: return "2p";
		case RiichiMahjongTile::PIN_3: return "3p";
		case RiichiMahjongTile::PIN_4: return "4p";
		case RiichiMahjongTile::PIN_5: return "5p";
		case RiichiMahjongTile::PIN_6: return "6p";
		case RiichiMahjongTile::PIN_7: return "7p";
		case RiichiMahjongTile::PIN_8: return "8p";
		case RiichiMahjongTile::PIN_9: return "9p";

			// Souzu (Bamboo)
		case RiichiMahjongTile::SOU_1: return "1s";
		case RiichiMahjongTile::SOU_2: return "2s";
		case RiichiMahjongTile::SOU_3: return "3s";
		case RiichiMahjongTile::SOU_4: return "4s";
		case RiichiMahjongTile::SOU_5: return "5s";
		case RiichiMahjongTile::SOU_6: return "6s";
		case RiichiMahjongTile::SOU_7: return "7s";
		case RiichiMahjongTile::SOU_8: return "8s";
		case RiichiMahjongTile::SOU_9: return "9s";

			// Winds
		case RiichiMahjongTile::EAST:  return "E";
		case RiichiMahjongTile::SOUTH: return "S";
		case RiichiMahjongTile::WEST:  return "W";
		case RiichiMahjongTile::NORTH: return "N";

			// Dragons
		case RiichiMahjongTile::RED_DRAGON:   return "RD";
		case RiichiMahjongTile::WHITE_DRAGON: return "WD";
		case RiichiMahjongTile::GREEN_DRAGON: return "GD";

			// Fallback
		default: return "?";

	}
}

std::string Tile::ToTenhouString() const
{
    return std::string();
}

void Tile::determineSuit()
{
	if ( IsManzu() )
	{
		suit = Suit::MANZU;
	}
	else if ( IsPinzu() )
	{
		suit = Suit::PINZU;
	}
	else if ( IsSouzu() )
	{
		suit = Suit::SOUZU;
	}
	else
	{
		suit = Suit::HONOR;
	}
}

bool Tile::AlwaysReturnTrue()
{
    return true;
}

#endif // LRM_DEBUG_API
