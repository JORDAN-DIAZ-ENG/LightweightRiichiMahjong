#pragma once

#include <cstdint>

#include "GameState.h"
#include "HandParse.h"
#include "Meld.h"
#include "Rules.h"
#include "Tile.h"

namespace LRMahjong::Model
{
	// Bit positions in ScoreResult::yaku.
	enum class Yaku : uint8_t
	{
		RIICHI = 0,
		IPPATSU,
		MENZEN_TSUMO,
		PINFU,
		TANYAO,
		IIPEIKO,
		YAKUHAI_HAKU,
		YAKUHAI_HATSU,
		YAKUHAI_CHUN,
		YAKUHAI_SEAT,
		YAKUHAI_ROUND,
		RINSHAN,
		CHANKAN,
		HAITEI,
		HOUTEI,

		DOUBLE_RIICHI,
		CHIITOITSU,
		SANSHOKU_DOUJUN,
		ITTSUU,
		CHANTA,
		SANSHOKU_DOUKOU,
		SANKANTSU,
		TOITOI,
		SANANKOU,
		SHOUSANGEN,
		HONROUTOU,

		HONITSU,
		JUNCHAN,
		RYANPEIKOU,

		CHINITSU,

		// Yakuman from here down; these do not add han, they replace it.
		KOKUSHI,
		SUUANKOU,
		DAISANGEN,
		SHOUSUUSHII,
		DAISUUSHII,
		TSUUIISOU,
		CHINROUTOU,
		RYUUIISOU,
		CHUUREN,
		SUUKANTSU,

		COUNT,
	};

	inline constexpr uint64_t YakuBit( const Yaku y ) { return 1ULL << static_cast<uint8_t>( y ); }

	LRM_API const char *YakuName( Yaku y );

	// Everything the scorer needs that is not in the tiles themselves.
	struct WinContext
	{
		Rules rules{};

		TileInstance winningTile = INVALID_INSTANCE;
		bool         byTsumo     = false;

		uint8_t seatWind  = 0; // 0 = east
		uint8_t roundWind = 0;
		bool    isDealer  = false;

		bool riichi       = false;
		bool doubleRiichi = false;
		bool ippatsu      = false;

		bool haitei  = false;
		bool houtei  = false;
		bool rinshan = false;
		bool chankan = false;

		TileInstance doraIndicators[MAX_DORA_INDICATORS]{};
		uint8_t      doraIndicatorCount = 0;
		TileInstance uraIndicators[MAX_DORA_INDICATORS]{};
		uint8_t      uraIndicatorCount = 0;

		uint8_t akaCount = 0; // red fives in hand and melds
		uint8_t nukiCount = 0; // sanma: pulled Norths, each worth a dora

		uint8_t honba        = 0;
		uint8_t riichiSticks = 0;
	};

	struct ScoreResult
	{
		// False when the hand is a complete shape holding no yaku, which is not
		// a win at all. M2 offers tsumo and ron on shape alone; this is what
		// turns that into a real answer.
		bool valid = false;

		uint64_t yaku = 0;

		uint8_t han          = 0; // including dora
		uint8_t fu           = 0;
		uint8_t doraHan      = 0;
		uint8_t yakumanCount = 0; // 0 when the hand is scored on han

		// What the winner collects in total, including honba and any riichi
		// sticks on the table.
		int32_t points = 0;

		// The breakdown. On a ron only fromDiscarder is set; on a tsumo the
		// other two are.
		int32_t fromDiscarder = 0;
		int32_t fromDealer    = 0;
		int32_t fromEachOther = 0;
	};

	// Scores a complete hand, taking the best reading of it. Returns a result
	// with valid == false if the hand holds no yaku.
	LRM_API ScoreResult ScoreHand( const Counts34 &concealed, const Meld *melds, uint8_t meldCount,
		const WinContext &context );

	// Cheap yes/no for action generation: does this hand hold a yaku at all.
	LRM_API bool HasYaku( const Counts34 &concealed, const Meld *melds, uint8_t meldCount,
		const WinContext &context );

	// Builds the context for a seat from live state, reading the winning tile,
	// the indicators, the winds and the flags off the hand result.
	LRM_API WinContext MakeWinContext( const GameState &state, uint8_t winner );

	// How many dora a tile is worth given one indicator, following the
	// indicator-points-at-the-next-tile rule and wrapping within its own group.
	LRM_API TileId DoraFromIndicator( TileId indicator );

} // namespace LRMahjong::Model
