#pragma once

#include <cstdint>
namespace LRMahjong::Model
{
	enum class RiichiMahjongTile : uint8_t
	{
		// Man ( Numbers )
		MAN_1, MAN_2, MAN_3, MAN_4, MAN_5, MAN_6, MAN_7, MAN_8, MAN_9,

		// Pin ( Dots / circles )
		PIN_1, PIN_2, PIN_3, PIN_4, PIN_5, PIN_6, PIN_7, PIN_8, PIN_9,

		// Souzu (Bamboo)
		SOU_1, SOU_2, SOU_3, SOU_4, SOU_5, SOU_6, SOU_7, SOU_8, SOU_9,

		// Winds
		EAST, SOUTH, WEST, NORTH,

		// Dragons
		RED_DRAGON, WHITE_DRAGON, GREEN_DRAGON,

		TILE_COUNT // 34 total
	};

	struct Tile
	{
		RiichiMahjongTile type;

		inline bool IsManzu()  { return type >= RiichiMahjongTile::MAN_1 && type <= RiichiMahjongTile::MAN_9; }
		inline bool IsPinzu()  { return type >= RiichiMahjongTile::PIN_1 && type <= RiichiMahjongTile::PIN_9; }
		inline bool IsSouzu()  { return type >= RiichiMahjongTile::SOU_1 && type <= RiichiMahjongTile::SOU_9; }
		inline bool IsWind()   { return type >= RiichiMahjongTile::EAST && type <= RiichiMahjongTile::NORTH; }
		inline bool IsDragon() { return type >= RiichiMahjongTile::RED_DRAGON && type <= RiichiMahjongTile::GREEN_DRAGON; }
		inline bool IsHonor()  { return type >= RiichiMahjongTile::EAST; }
	};
} // namespace LRMahjong::Model