#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "../Core.h"

namespace LRMahjong::Model
{
	// A TileId IS the index into every 34-slot table in the engine, so the
	// ordering below is canonical and must not be changed:
	//
	//    0 ..  8   manzu 1-9
	//    9 .. 17   pinzu 1-9
	//   18 .. 26   souzu 1-9
	//   27 .. 30   east, south, west, north
	//   31 .. 33   white dragon (haku), green dragon (hatsu), red dragon (chun)
	//
	// This matches the Tenhou 'z' ordering, so honour digits are simply
	// ( id - 26 ). Sanma keeps all 34 slots and pins MAN_2..MAN_8 to zero, which
	// lets shanten, ukeire and yaku share one code path across both formats.
	using TileId = uint8_t;

	inline constexpr TileId TILE_KIND_COUNT = 34;
	inline constexpr TileId INVALID_TILE    = 0x7F;

	enum class RiichiMahjongTile : TileId
	{
		// Man ( Numbers )
		MAN_1 = 0, MAN_2, MAN_3, MAN_4, MAN_5, MAN_6, MAN_7, MAN_8, MAN_9,

		// Pin ( Dots / circles )
		PIN_1, PIN_2, PIN_3, PIN_4, PIN_5, PIN_6, PIN_7, PIN_8, PIN_9,

		// Souzu ( Bamboo )
		SOU_1, SOU_2, SOU_3, SOU_4, SOU_5, SOU_6, SOU_7, SOU_8, SOU_9,

		// Winds
		EAST, SOUTH, WEST, NORTH,

		// Dragons, in canonical order: haku, hatsu, chun
		WHITE_DRAGON, GREEN_DRAGON, RED_DRAGON,

		TILE_COUNT // 34 total
	};

	enum class Suit : uint8_t
	{
		MANZU     = 0,
		PINZU     = 1,
		SOUZU     = 2,
		HONOR     = 3,
		UNDEFINED = 4,
	};

	inline constexpr TileId Id( const RiichiMahjongTile kind ) { return static_cast<TileId>( kind ); }
	inline constexpr RiichiMahjongTile Kind( const TileId id ) { return static_cast<RiichiMahjongTile>( id ); }

	// ---------------------------------------------------------------------
	// Tile predicates. Free, constexpr and header-only: these sit inside the
	// innermost loops of shanten and ukeire, so they must never become a call
	// across the DLL boundary.
	// ---------------------------------------------------------------------
	inline constexpr bool IsValidTile( const TileId t ) { return t < TILE_KIND_COUNT; }
	inline constexpr bool IsManzu( const TileId t )     { return t < 9; }
	inline constexpr bool IsPinzu( const TileId t )     { return t >= 9 && t < 18; }
	inline constexpr bool IsSouzu( const TileId t )     { return t >= 18 && t < 27; }
	inline constexpr bool IsSuited( const TileId t )    { return t < 27; }
	inline constexpr bool IsHonor( const TileId t )     { return t >= 27 && t < TILE_KIND_COUNT; }
	inline constexpr bool IsWind( const TileId t )      { return t >= 27 && t < 31; }
	inline constexpr bool IsDragon( const TileId t )    { return t >= 31 && t < TILE_KIND_COUNT; }

	// 1-9 for suited tiles, 0 for honours.
	inline constexpr uint8_t RankOf( const TileId t )
	{
		return IsSuited( t ) ? static_cast<uint8_t>( t % 9 + 1 ) : static_cast<uint8_t>( 0 );
	}

	inline constexpr Suit SuitOf( const TileId t )
	{
		if ( IsSuited( t ) ) return static_cast<Suit>( t / 9 );
		return IsHonor( t ) ? Suit::HONOR : Suit::UNDEFINED;
	}

	inline constexpr bool IsTerminal( const TileId t )        { return IsSuited( t ) && ( t % 9 == 0 || t % 9 == 8 ); }
	inline constexpr bool IsTerminalOrHonor( const TileId t ) { return IsHonor( t ) || IsTerminal( t ); }
	inline constexpr bool IsSimple( const TileId t )          { return IsSuited( t ) && !IsTerminal( t ); }

	// MAN_2..MAN_8 do not exist in three-player mahjong.
	inline constexpr bool IsSanmaTile( const TileId t ) { return IsValidTile( t ) && !( t >= 1 && t <= 7 ); }

	// ---------------------------------------------------------------------
	// Red fives.
	//
	// Aka dora are deliberately kept out of the 34-tile space: they only ever
	// affect dora counting, so shanten and yaku code should never have to know
	// about them. A hand or meld carries a 3-bit mask alongside its counts.
	// ---------------------------------------------------------------------
	inline constexpr TileId MAN_5_ID = 4;
	inline constexpr TileId PIN_5_ID = 13;
	inline constexpr TileId SOU_5_ID = 22;

	using AkaMask = uint8_t;
	inline constexpr AkaMask AKA_NONE = 0;
	inline constexpr AkaMask AKA_MAN5 = 1 << 0;
	inline constexpr AkaMask AKA_PIN5 = 1 << 1;
	inline constexpr AkaMask AKA_SOU5 = 1 << 2;

	// The aka bit belonging to a tile kind, or AKA_NONE if it has no red variant.
	inline constexpr AkaMask AkaBitFor( const TileId t )
	{
		if ( t == MAN_5_ID ) return AKA_MAN5;
		if ( t == PIN_5_ID ) return AKA_PIN5;
		if ( t == SOU_5_ID ) return AKA_SOU5;
		return AKA_NONE;
	}

	// ---------------------------------------------------------------------
	// A physical tile: bits 0-6 hold the TileId, bit 7 marks a red five.
	//
	// Packed into one byte because the 136-slot wall is memcpy'd on every
	// rollout; two bytes per tile would double the cost of the copy.
	// ---------------------------------------------------------------------
	using TileInstance = uint8_t;
	inline constexpr TileInstance AKA_BIT          = 0x80;
	inline constexpr TileInstance INVALID_INSTANCE = INVALID_TILE;

	inline constexpr TileInstance MakeInstance( const TileId t, const bool aka = false )
	{
		return static_cast<TileInstance>( aka ? ( t | AKA_BIT ) : t );
	}
	inline constexpr TileId InstanceTile( const TileInstance i )  { return static_cast<TileId>( i & 0x7F ); }
	inline constexpr bool   InstanceIsAka( const TileInstance i ) { return ( i & AKA_BIT ) != 0; }
	inline constexpr bool   IsValidInstance( const TileInstance i ) { return IsValidTile( InstanceTile( i ) ); }

	// The workhorse representation: how many of each kind a hand holds.
	using Counts34 = std::array<uint8_t, TILE_KIND_COUNT>;

	// ---------------------------------------------------------------------
	// Text conversion. Not hot-path, so these live in Tile.cpp behind LRM_API.
	//
	// Tenhou notation groups digits by suit ( "123m456p11z" ) and writes a red
	// five as the digit 0.
	// ---------------------------------------------------------------------

	// Human readable: "1m", "E", "RD".
	LRM_API std::string TileToString( TileId t );

	// Tenhou form for a single tile: "1m".."9s", "1z".."7z".
	LRM_API std::string TileToTenhouString( TileId t );

	// Parses a single Tenhou tile. Returns INVALID_TILE if unparseable.
	LRM_API TileId TileFromTenhouString( std::string_view text );

	LRM_API char SuitToTenhouChar( Suit suit );

	LRM_API std::string CountsToTenhouString( const Counts34 &counts, AkaMask aka = AKA_NONE );

	// Returns false and leaves the outputs untouched if the string is malformed
	// or asks for a fifth copy of a tile.
	LRM_API bool CountsFromTenhouString( std::string_view text, Counts34 &outCounts, AkaMask &outAka );

	// Debug-only export, used to verify the LRM_DEBUG_API plumbing.
	LRM_DEBUG_API bool AlwaysReturnTrue();

} // namespace LRMahjong::Model
