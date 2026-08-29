#include "Score.h"

#include "WinCheck.h"

namespace LRMahjong::Model
{
	namespace
	{
		constexpr uint64_t YAKUMAN_MASK =
			YakuBit( Yaku::KOKUSHI )     | YakuBit( Yaku::SUUANKOU )   | YakuBit( Yaku::DAISANGEN ) |
			YakuBit( Yaku::SHOUSUUSHII ) | YakuBit( Yaku::DAISUUSHII ) | YakuBit( Yaku::TSUUIISOU ) |
			YakuBit( Yaku::CHINROUTOU )  | YakuBit( Yaku::RYUUIISOU )  | YakuBit( Yaku::CHUUREN )   |
			YakuBit( Yaku::SUUKANTSU );

		// Green dragon plus the all-green souzu.
		bool IsGreenTile( const TileId t )
		{
			if ( t == Id( RiichiMahjongTile::GREEN_DRAGON ) ) return true;
			if ( !IsSouzu( t ) ) return false;

			const uint8_t rank = RankOf( t );
			return rank == 2 || rank == 3 || rank == 4 || rank == 6 || rank == 8;
		}

		// Every tile the hand holds, melds included.
		Counts34 FullHand( const Counts34 &concealed, const Meld *melds, const uint8_t meldCount )
		{
			Counts34 all = concealed;
			for ( uint8_t i = 0; i < meldCount; ++i ) melds[i].AddTo( all );
			return all;
		}

		bool IsMenzen( const Meld *melds, const uint8_t meldCount )
		{
			for ( uint8_t i = 0; i < meldCount; ++i )
			{
				if ( !melds[i].IsConcealed() ) return false;
			}
			return true;
		}

		// ---------------------------------------------------------------
		// Fu
		// ---------------------------------------------------------------

		uint8_t SetFu( const HandSet &set )
		{
			if ( !set.IsTripletLike() ) return 0;

			const bool honor = IsTerminalOrHonor( set.tile );

			if ( set.type == SetType::KAN )
			{
				if ( set.concealed ) return honor ? 32 : 16;
				return honor ? 16 : 8;
			}

			if ( set.concealed ) return honor ? 8 : 4;
			return honor ? 4 : 2;
		}

		// A pair of dragons, or of the seat or round wind, is worth two.
		uint8_t PairFu( const TileId pair, const WinContext &ctx )
		{
			uint8_t fu = 0;

			if ( IsDragon( pair ) ) fu = static_cast<uint8_t>( fu + 2 );

			const TileId seat  = static_cast<TileId>( Id( RiichiMahjongTile::EAST ) + ctx.seatWind );
			const TileId round = static_cast<TileId>( Id( RiichiMahjongTile::EAST ) + ctx.roundWind );

			// A double wind counts twice. Whether Mahjong Soul agrees is one of
			// the unverified rules; the log corpus settles it.
			if ( pair == seat )  fu = static_cast<uint8_t>( fu + 2 );
			if ( pair == round ) fu = static_cast<uint8_t>( fu + 2 );

			return fu;
		}

		// True when the winning tile completed the set as a two-sided run wait.
		bool IsOpenWait( const HandSet &set, const TileId winning )
		{
			if ( set.type != SetType::RUN ) return false;

			// A run waiting in the middle is a closed wait, and one anchored on
			// a terminal only ever had one side.
			if ( winning == set.tile + 1 ) return false;                 // kanchan
			if ( winning == set.tile && RankOf( set.tile ) == 7 ) return false; // 789 waiting on 7
			if ( winning == set.tile + 2 && RankOf( set.tile ) == 1 ) return false; // 123 waiting on 3

			return true;
		}

		struct FuBreakdown
		{
			uint8_t fu = 0;
			bool    pinfuShape = false;
		};

		FuBreakdown ComputeFu( const Decomposition &d, const Counts34 &concealed,
			const Meld *melds, const uint8_t meldCount, const WinContext &ctx )
		{
			FuBreakdown result;

			if ( d.sevenPairs )
			{
				result.fu = 25; // flat, and nothing else applies
				return result;
			}

			const bool menzen  = IsMenzen( melds, meldCount );
			const TileId winning = InstanceTile( ctx.winningTile );

			uint8_t fu = 20;
			uint8_t setFu = 0;
			uint8_t pairFu = 0;
			bool allRuns = true;

			for ( uint8_t i = 0; i < d.setCount; ++i )
			{
				const HandSet &set = d.sets[i];

				if ( set.type == SetType::PAIR )
				{
					pairFu = PairFu( set.tile, ctx );
					continue;
				}

				if ( set.IsTripletLike() ) allRuns = false;
				setFu = static_cast<uint8_t>( setFu + SetFu( set ) );
			}

			// The wait is worth two unless it was two-sided or a shanpon.
			uint8_t waitFu = 2;
			bool    openWait = false;

			for ( uint8_t i = 0; i < d.setCount; ++i )
			{
				const HandSet &set = d.sets[i];
				if ( set.fromCall ) continue;

				if ( set.type == SetType::RUN &&
					winning >= set.tile && winning <= set.tile + 2 &&
					IsOpenWait( set, winning ) )
				{
					openWait = true;
					break;
				}

				// Completing the triplet from a discard is a shanpon, also zero.
				if ( set.type == SetType::TRIPLET && set.tile == winning )
				{
					openWait = true;
					break;
				}
			}

			if ( openWait ) waitFu = 0;

			// Pinfu: closed, every set a run, a valueless pair and a two-sided
			// wait. Nothing but the base twenty.
			const bool pinfu = menzen && allRuns && pairFu == 0 && openWait && setFu == 0;
			result.pinfuShape = pinfu;

			if ( pinfu )
			{
				result.fu = ctx.byTsumo ? static_cast<uint8_t>( 20 ) : static_cast<uint8_t>( 30 );
				return result;
			}

			fu = static_cast<uint8_t>( fu + setFu + pairFu + waitFu );

			if ( ctx.byTsumo )            fu = static_cast<uint8_t>( fu + 2 );
			else if ( menzen )            fu = static_cast<uint8_t>( fu + 10 );

			// An open hand with no fu at all still scores the minimum.
			if ( !menzen && !ctx.byTsumo && fu == 20 ) fu = 30;

			// Round up to the next ten.
			result.fu = static_cast<uint8_t>( ( ( fu + 9 ) / 10 ) * 10 );

			( void )concealed;
			return result;
		}

		// ---------------------------------------------------------------
		// Yaku
		// ---------------------------------------------------------------

		uint8_t CountConcealedTriplets( const Decomposition &d, const WinContext &ctx )
		{
			const TileId winning = InstanceTile( ctx.winningTile );

			uint8_t count = 0;
			for ( uint8_t i = 0; i < d.setCount; ++i )
			{
				const HandSet &set = d.sets[i];
				if ( !set.IsTripletLike() || !set.concealed ) continue;

				// A triplet completed by someone else's discard was never
				// concealed, even though it sits in a closed hand.
				if ( !ctx.byTsumo && set.type == SetType::TRIPLET && set.tile == winning ) continue;

				++count;
			}
			return count;
		}

		uint64_t DetectYakuman( const Decomposition &d, const Counts34 &all, const WinContext &ctx,
			const Meld *melds, const uint8_t meldCount )
		{
			uint64_t yaku = 0;

			if ( d.thirteenOrphans )
			{
				return YakuBit( Yaku::KOKUSHI );
			}

			const bool menzen = IsMenzen( melds, meldCount );

			// Four concealed triplets.
			if ( !d.sevenPairs && menzen && CountConcealedTriplets( d, ctx ) == 4 )
			{
				yaku |= YakuBit( Yaku::SUUANKOU );
			}

			// Dragons and winds.
			uint8_t dragonSets = 0;
			uint8_t windSets   = 0;
			uint8_t dragonPair = 0;
			uint8_t windPair   = 0;

			for ( uint8_t i = 0; i < d.setCount; ++i )
			{
				const HandSet &set = d.sets[i];
				const bool isPair = set.type == SetType::PAIR;

				if ( IsDragon( set.tile ) ) { if ( isPair ) ++dragonPair; else ++dragonSets; }
				if ( IsWind( set.tile ) )   { if ( isPair ) ++windPair;   else ++windSets; }
			}

			if ( !d.sevenPairs )
			{
				if ( dragonSets == 3 )                    yaku |= YakuBit( Yaku::DAISANGEN );
				if ( windSets == 4 )                      yaku |= YakuBit( Yaku::DAISUUSHII );
				else if ( windSets == 3 && windPair == 1 ) yaku |= YakuBit( Yaku::SHOUSUUSHII );
			}

			// Whole-hand composition tests read the tiles directly, so they hold
			// for seven pairs too.
			bool allHonors    = true;
			bool allTerminals = true;
			bool allGreen     = true;

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				if ( all[t] == 0 ) continue;

				if ( !IsHonor( t ) )           allHonors = false;
				if ( !IsTerminal( t ) )        allTerminals = false;
				if ( !IsGreenTile( t ) )       allGreen = false;
			}

			if ( allHonors )    yaku |= YakuBit( Yaku::TSUUIISOU );
			if ( allTerminals ) yaku |= YakuBit( Yaku::CHINROUTOU );
			if ( allGreen )     yaku |= YakuBit( Yaku::RYUUIISOU );

			// Four kans.
			uint8_t kans = 0;
			for ( uint8_t i = 0; i < meldCount; ++i )
			{
				if ( melds[i].IsKan() ) ++kans;
			}
			if ( kans == 4 ) yaku |= YakuBit( Yaku::SUUKANTSU );

			// Nine gates: a closed flush of 1112345678999 plus any one tile.
			if ( menzen && !d.sevenPairs )
			{
				int suit = -1;
				bool oneSuit = true;

				for ( TileId t = 0; t < TILE_KIND_COUNT && oneSuit; ++t )
				{
					if ( all[t] == 0 ) continue;
					if ( IsHonor( t ) ) { oneSuit = false; break; }

					const int s = t / 9;
					if ( suit < 0 ) suit = s;
					else if ( suit != s ) oneSuit = false;
				}

				if ( oneSuit && suit >= 0 )
				{
					const TileId base = static_cast<TileId>( suit * 9 );
					bool gates = all[base] >= 3 && all[base + 8] >= 3;

					for ( int r = 1; r <= 7 && gates; ++r )
					{
						if ( all[base + r] < 1 ) gates = false;
					}

					if ( gates ) yaku |= YakuBit( Yaku::CHUUREN );
				}
			}

			return yaku;
		}

		uint64_t DetectYaku( const Decomposition &d, const Counts34 &all, const WinContext &ctx,
			const Meld *melds, const uint8_t meldCount, const bool pinfuShape )
		{
			uint64_t yaku = 0;

			const bool menzen  = IsMenzen( melds, meldCount );
			const TileId winning = InstanceTile( ctx.winningTile );

			// ---- circumstantial -------------------------------------
			if ( ctx.riichi )
			{
				yaku |= ctx.doubleRiichi ? YakuBit( Yaku::DOUBLE_RIICHI ) : YakuBit( Yaku::RIICHI );
				if ( ctx.ippatsu ) yaku |= YakuBit( Yaku::IPPATSU );
			}

			if ( ctx.byTsumo && menzen ) yaku |= YakuBit( Yaku::MENZEN_TSUMO );
			if ( ctx.rinshan )           yaku |= YakuBit( Yaku::RINSHAN );
			if ( ctx.chankan )           yaku |= YakuBit( Yaku::CHANKAN );
			if ( ctx.haitei )            yaku |= YakuBit( Yaku::HAITEI );
			if ( ctx.houtei )            yaku |= YakuBit( Yaku::HOUTEI );

			if ( d.thirteenOrphans ) return yaku;

			if ( d.sevenPairs ) yaku |= YakuBit( Yaku::CHIITOITSU );

			// ---- composition ----------------------------------------
			bool allSimples      = true;
			bool everySetTouches = true; // chanta and junchan
			bool everySetTerminal = true;
			bool anyHonor        = false;
			int  suit            = -1;
			bool oneSuit         = true;

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				if ( all[t] == 0 ) continue;

				if ( IsTerminalOrHonor( t ) ) allSimples = false;
				if ( IsHonor( t ) ) anyHonor = true;
				else
				{
					const int s = t / 9;
					if ( suit < 0 ) suit = s;
					else if ( suit != s ) oneSuit = false;
				}
			}

			if ( allSimples && ( ctx.rules.kuitan || menzen ) ) yaku |= YakuBit( Yaku::TANYAO );

			if ( oneSuit && suit >= 0 )
			{
				if ( !anyHonor ) yaku |= YakuBit( Yaku::CHINITSU );
				else             yaku |= YakuBit( Yaku::HONITSU );
			}

			if ( d.sevenPairs )
			{
				// Honroutou can still apply to seven pairs.
				bool allTerminalOrHonor = true;
				for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
				{
					if ( all[t] != 0 && !IsTerminalOrHonor( t ) ) allTerminalOrHonor = false;
				}
				if ( allTerminalOrHonor ) yaku |= YakuBit( Yaku::HONROUTOU );

				return yaku;
			}

			// ---- yakuhai --------------------------------------------
			const TileId seatTile  = static_cast<TileId>( Id( RiichiMahjongTile::EAST ) + ctx.seatWind );
			const TileId roundTile = static_cast<TileId>( Id( RiichiMahjongTile::EAST ) + ctx.roundWind );

			uint8_t dragonSets = 0;
			uint8_t dragonPairs = 0;
			uint8_t tripletCount = 0;
			uint8_t kanCount = 0;

			for ( uint8_t i = 0; i < d.setCount; ++i )
			{
				const HandSet &set = d.sets[i];

				if ( set.type == SetType::PAIR )
				{
					if ( IsDragon( set.tile ) ) ++dragonPairs;
					if ( !IsTerminalOrHonor( set.tile ) ) everySetTouches = false;
					if ( !IsTerminal( set.tile ) ) everySetTerminal = false;
					continue;
				}

				if ( set.IsTripletLike() ) ++tripletCount;
				if ( set.type == SetType::KAN ) ++kanCount;

				if ( set.IsTripletLike() )
				{
					if ( set.tile == Id( RiichiMahjongTile::WHITE_DRAGON ) ) yaku |= YakuBit( Yaku::YAKUHAI_HAKU );
					if ( set.tile == Id( RiichiMahjongTile::GREEN_DRAGON ) ) yaku |= YakuBit( Yaku::YAKUHAI_HATSU );
					if ( set.tile == Id( RiichiMahjongTile::RED_DRAGON ) )   yaku |= YakuBit( Yaku::YAKUHAI_CHUN );
					if ( set.tile == seatTile )  yaku |= YakuBit( Yaku::YAKUHAI_SEAT );
					if ( set.tile == roundTile ) yaku |= YakuBit( Yaku::YAKUHAI_ROUND );

					if ( IsDragon( set.tile ) ) ++dragonSets;
				}

				// Chanta needs every block to contain a terminal or honour;
				// junchan needs a terminal specifically.
				const bool touchesTerminal = set.type == SetType::RUN
					? ( RankOf( set.tile ) == 1 || RankOf( set.tile ) == 7 )
					: IsTerminalOrHonor( set.tile );

				const bool touchesTerminalOnly = set.type == SetType::RUN
					? ( RankOf( set.tile ) == 1 || RankOf( set.tile ) == 7 )
					: IsTerminal( set.tile );

				if ( !touchesTerminal )     everySetTouches = false;
				if ( !touchesTerminalOnly ) everySetTerminal = false;
			}

			if ( dragonSets == 2 && dragonPairs == 1 ) yaku |= YakuBit( Yaku::SHOUSANGEN );

			// ---- shape ----------------------------------------------
			if ( pinfuShape ) yaku |= YakuBit( Yaku::PINFU );

			if ( tripletCount == 4 ) yaku |= YakuBit( Yaku::TOITOI );

			const uint8_t concealedTriplets = CountConcealedTriplets( d, ctx );
			if ( concealedTriplets == 3 ) yaku |= YakuBit( Yaku::SANANKOU );

			if ( kanCount == 3 ) yaku |= YakuBit( Yaku::SANKANTSU );

			// Identical runs.
			uint8_t identicalPairs = 0;
			for ( uint8_t i = 0; i < d.setCount; ++i )
			{
				if ( d.sets[i].type != SetType::RUN ) continue;

				for ( uint8_t j = static_cast<uint8_t>( i + 1 ); j < d.setCount; ++j )
				{
					if ( d.sets[j].type == SetType::RUN && d.sets[j].tile == d.sets[i].tile ) ++identicalPairs;
				}
			}

			if ( menzen )
			{
				if ( identicalPairs >= 2 )      yaku |= YakuBit( Yaku::RYANPEIKOU );
				else if ( identicalPairs == 1 ) yaku |= YakuBit( Yaku::IIPEIKO );
			}

			// Three colours, runs or triplets.
			for ( uint8_t i = 0; i < d.setCount; ++i )
			{
				const HandSet &set = d.sets[i];
				if ( set.type == SetType::PAIR || !IsSuited( set.tile ) ) continue;

				const uint8_t rank = RankOf( set.tile );

				bool haveRun[3] = {};
				bool haveTri[3] = {};

				for ( uint8_t j = 0; j < d.setCount; ++j )
				{
					const HandSet &other = d.sets[j];
					if ( other.type == SetType::PAIR || !IsSuited( other.tile ) ) continue;
					if ( RankOf( other.tile ) != rank ) continue;

					const int s = other.tile / 9;
					if ( other.type == SetType::RUN ) haveRun[s] = true;
					else                              haveTri[s] = true;
				}

				if ( haveRun[0] && haveRun[1] && haveRun[2] ) yaku |= YakuBit( Yaku::SANSHOKU_DOUJUN );
				if ( haveTri[0] && haveTri[1] && haveTri[2] ) yaku |= YakuBit( Yaku::SANSHOKU_DOUKOU );
			}

			// A straight through one suit.
			for ( int s = 0; s < 3; ++s )
			{
				bool first = false, second = false, third = false;

				for ( uint8_t i = 0; i < d.setCount; ++i )
				{
					const HandSet &set = d.sets[i];
					if ( set.type != SetType::RUN || set.tile / 9 != s ) continue;

					const uint8_t rank = RankOf( set.tile );
					if ( rank == 1 ) first = true;
					if ( rank == 4 ) second = true;
					if ( rank == 7 ) third = true;
				}

				if ( first && second && third ) yaku |= YakuBit( Yaku::ITTSUU );
			}

			if ( everySetTouches )
			{
				if ( everySetTerminal )   yaku |= YakuBit( Yaku::JUNCHAN );
				else if ( anyHonor )      yaku |= YakuBit( Yaku::CHANTA );

				// All terminals and honours, with no runs at all.
				bool allTerminalOrHonor = true;
				for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
				{
					if ( all[t] != 0 && !IsTerminalOrHonor( t ) ) allTerminalOrHonor = false;
				}
				if ( allTerminalOrHonor && tripletCount == 4 ) yaku |= YakuBit( Yaku::HONROUTOU );
			}

			( void )winning;
			return yaku;
		}

		// ---------------------------------------------------------------
		// Han
		// ---------------------------------------------------------------

		uint8_t HanFor( const Yaku y, const bool menzen )
		{
			switch ( y )
			{
			case Yaku::RIICHI:
			case Yaku::IPPATSU:
			case Yaku::MENZEN_TSUMO:
			case Yaku::PINFU:
			case Yaku::TANYAO:
			case Yaku::IIPEIKO:
			case Yaku::YAKUHAI_HAKU:
			case Yaku::YAKUHAI_HATSU:
			case Yaku::YAKUHAI_CHUN:
			case Yaku::YAKUHAI_SEAT:
			case Yaku::YAKUHAI_ROUND:
			case Yaku::RINSHAN:
			case Yaku::CHANKAN:
			case Yaku::HAITEI:
			case Yaku::HOUTEI:
				return 1;

			case Yaku::DOUBLE_RIICHI:
			case Yaku::CHIITOITSU:
			case Yaku::SANSHOKU_DOUKOU:
			case Yaku::SANKANTSU:
			case Yaku::TOITOI:
			case Yaku::SANANKOU:
			case Yaku::SHOUSANGEN:
			case Yaku::HONROUTOU:
				return 2;

			// These lose a han when the hand is open.
			case Yaku::SANSHOKU_DOUJUN:
			case Yaku::ITTSUU:
			case Yaku::CHANTA:
				return menzen ? 2 : 1;

			case Yaku::HONITSU:
			case Yaku::JUNCHAN:
				return menzen ? 3 : 2;

			case Yaku::RYANPEIKOU:
				return 3;

			case Yaku::CHINITSU:
				return menzen ? 6 : 5;

			default:
				return 0;
			}
		}

		// ---------------------------------------------------------------
		// Dora
		// ---------------------------------------------------------------

		uint8_t CountDora( const Counts34 &all, const TileInstance *indicators, const uint8_t count )
		{
			uint8_t dora = 0;

			for ( uint8_t i = 0; i < count; ++i )
			{
				const TileId indicator = InstanceTile( indicators[i] );
				if ( !IsValidTile( indicator ) ) continue;

				dora = static_cast<uint8_t>( dora + all[DoraFromIndicator( indicator )] );
			}

			return dora;
		}

		// ---------------------------------------------------------------
		// Points
		// ---------------------------------------------------------------

		int32_t RoundUpTo100( const int32_t value )
		{
			return ( ( value + 99 ) / 100 ) * 100;
		}

		int32_t BasePoints( const uint8_t han, const uint8_t fu, const uint8_t yakumanCount,
			const Rules &rules )
		{
			if ( yakumanCount > 0 ) return 8000 * yakumanCount;

			if ( han >= 13 ) return 8000; // counted yakuman
			if ( han >= 11 ) return 6000;
			if ( han >= 8 )  return 4000;
			if ( han >= 6 )  return 3000;
			if ( han == 5 )  return 2000;

			if ( rules.kiriageMangan &&
				( ( han == 4 && fu == 30 ) || ( han == 3 && fu == 60 ) ) )
			{
				return 2000;
			}

			const int32_t base = static_cast<int32_t>( fu ) * ( 1 << ( 2 + han ) );
			return ( base > 2000 ) ? 2000 : base;
		}
	}

	const char *YakuName( const Yaku y )
	{
		switch ( y )
		{
		case Yaku::RIICHI:          return "riichi";
		case Yaku::IPPATSU:         return "ippatsu";
		case Yaku::MENZEN_TSUMO:    return "menzen tsumo";
		case Yaku::PINFU:           return "pinfu";
		case Yaku::TANYAO:          return "tanyao";
		case Yaku::IIPEIKO:         return "iipeiko";
		case Yaku::YAKUHAI_HAKU:    return "haku";
		case Yaku::YAKUHAI_HATSU:   return "hatsu";
		case Yaku::YAKUHAI_CHUN:    return "chun";
		case Yaku::YAKUHAI_SEAT:    return "seat wind";
		case Yaku::YAKUHAI_ROUND:   return "round wind";
		case Yaku::RINSHAN:         return "rinshan kaihou";
		case Yaku::CHANKAN:         return "chankan";
		case Yaku::HAITEI:          return "haitei raoyue";
		case Yaku::HOUTEI:          return "houtei raoyui";
		case Yaku::DOUBLE_RIICHI:   return "double riichi";
		case Yaku::CHIITOITSU:      return "chiitoitsu";
		case Yaku::SANSHOKU_DOUJUN: return "sanshoku doujun";
		case Yaku::ITTSUU:          return "ittsuu";
		case Yaku::CHANTA:          return "chanta";
		case Yaku::SANSHOKU_DOUKOU: return "sanshoku doukou";
		case Yaku::SANKANTSU:       return "sankantsu";
		case Yaku::TOITOI:          return "toitoi";
		case Yaku::SANANKOU:        return "sanankou";
		case Yaku::SHOUSANGEN:      return "shousangen";
		case Yaku::HONROUTOU:       return "honroutou";
		case Yaku::HONITSU:         return "honitsu";
		case Yaku::JUNCHAN:         return "junchan";
		case Yaku::RYANPEIKOU:      return "ryanpeikou";
		case Yaku::CHINITSU:        return "chinitsu";
		case Yaku::KOKUSHI:         return "kokushi musou";
		case Yaku::SUUANKOU:        return "suuankou";
		case Yaku::DAISANGEN:       return "daisangen";
		case Yaku::SHOUSUUSHII:     return "shousuushii";
		case Yaku::DAISUUSHII:      return "daisuushii";
		case Yaku::TSUUIISOU:       return "tsuuiisou";
		case Yaku::CHINROUTOU:      return "chinroutou";
		case Yaku::RYUUIISOU:       return "ryuuiisou";
		case Yaku::CHUUREN:         return "chuuren poutou";
		case Yaku::SUUKANTSU:       return "suukantsu";
		default:                    return "?";
		}
	}

	TileId DoraFromIndicator( const TileId indicator )
	{
		if ( !IsValidTile( indicator ) ) return INVALID_TILE;

		// The indicator points at the next tile along, wrapping inside its own
		// group: 9 back to 1, north back to east, chun back to haku.
		if ( IsSuited( indicator ) )
		{
			const TileId base = static_cast<TileId>( ( indicator / 9 ) * 9 );
			return static_cast<TileId>( base + ( indicator - base + 1 ) % 9 );
		}

		if ( IsWind( indicator ) )
		{
			const TileId base = Id( RiichiMahjongTile::EAST );
			return static_cast<TileId>( base + ( indicator - base + 1 ) % 4 );
		}

		const TileId base = Id( RiichiMahjongTile::WHITE_DRAGON );
		return static_cast<TileId>( base + ( indicator - base + 1 ) % 3 );
	}

	ScoreResult ScoreHand( const Counts34 &concealed, const Meld *melds, const uint8_t meldCount,
		const WinContext &context )
	{
		ScoreResult best;

		DecompositionList readings;
		if ( Decompose( concealed, melds, meldCount, readings ) == 0 ) return best;

		const Counts34 all    = FullHand( concealed, melds, meldCount );
		const bool     menzen = IsMenzen( melds, meldCount );

		// Dora never depends on the reading, so it is counted once.
		uint8_t dora = CountDora( all, context.doraIndicators, context.doraIndicatorCount );
		dora = static_cast<uint8_t>( dora + context.akaCount + context.nukiCount );
		if ( context.riichi )
		{
			dora = static_cast<uint8_t>( dora + CountDora( all, context.uraIndicators, context.uraIndicatorCount ) );
		}

		for ( const Decomposition &d : readings )
		{
			const FuBreakdown fu = ComputeFu( d, concealed, melds, meldCount, context );

			const uint64_t yakuman = DetectYakuman( d, all, context, melds, meldCount );
			uint64_t yaku = DetectYaku( d, all, context, melds, meldCount, fu.pinfuShape );

			ScoreResult candidate;
			candidate.fu = fu.fu;

			if ( yakuman != 0 )
			{
				// A yakuman replaces the han count outright, so the ordinary
				// yaku are dropped rather than added to it.
				candidate.yaku = yakuman;

				uint8_t count = 0;
				for ( uint8_t bit = 0; bit < static_cast<uint8_t>( Yaku::COUNT ); ++bit )
				{
					if ( ( yakuman & ( 1ULL << bit ) ) != 0 ) ++count;
				}

				candidate.yakumanCount = count;
				candidate.han = static_cast<uint8_t>( 13 * count );
				candidate.valid = true;
			}
			else
			{
				uint8_t han = 0;
				for ( uint8_t bit = 0; bit < static_cast<uint8_t>( Yaku::COUNT ); ++bit )
				{
					if ( ( yaku & ( 1ULL << bit ) ) == 0 ) continue;
					han = static_cast<uint8_t>( han + HanFor( static_cast<Yaku>( bit ), menzen ) );
				}

				// A complete shape with no yaku is not a win, and dora alone
				// never makes one.
				if ( han == 0 ) continue;

				candidate.yaku    = yaku;
				candidate.doraHan = dora;
				candidate.han     = static_cast<uint8_t>( han + dora );
				candidate.valid   = true;
			}

			// Best reading is the one worth most: yakuman first, then han, then
			// fu as the tie-break.
			const bool better =
				!best.valid ||
				candidate.yakumanCount > best.yakumanCount ||
				( candidate.yakumanCount == best.yakumanCount &&
					( candidate.han > best.han ||
						( candidate.han == best.han && candidate.fu > best.fu ) ) );

			if ( better ) best = candidate;
		}

		if ( !best.valid ) return best;

		// ---- turn han and fu into points -------------------------------
		const Rules &rules = context.rules;
		const int32_t base = BasePoints( best.han, best.fu, best.yakumanCount, rules );

		const int32_t honbaTotal = 300 * context.honba;
		const int32_t sticks     = 1000 * context.riichiSticks;

		if ( !context.byTsumo )
		{
			const int32_t value = RoundUpTo100( base * ( context.isDealer ? 6 : 4 ) );
			best.fromDiscarder = value + honbaTotal;
			best.points        = best.fromDiscarder + sticks;
			return best;
		}

		const int32_t payers    = rules.numPlayers - 1;
		const int32_t honbaEach = 100 * context.honba;

		if ( context.isDealer )
		{
			const int32_t each = RoundUpTo100( base * 2 ) + honbaEach;
			best.fromEachOther = each;
			best.points        = each * payers + sticks;
		}
		else
		{
			int32_t fromDealer = RoundUpTo100( base * 2 );
			int32_t fromKo     = RoundUpTo100( base * 1 );

			// Sanma has one fewer non-dealer to collect from. Under tsumo loss
			// the winner simply receives less; without it the missing share is
			// made up by those still at the table.
			//
			// UNVERIFIED: which of the two Mahjong Soul uses, and how it
			// distributes the shortfall. The log corpus settles it.
			if ( rules.numPlayers == 3 && !rules.tsumoLoss )
			{
				const int32_t missing = base; // the absent non-dealer's share
				fromDealer = RoundUpTo100( base * 2 + missing / 2 );
				fromKo     = RoundUpTo100( base * 1 + missing / 2 );
			}

			best.fromDealer    = fromDealer + honbaEach;
			best.fromEachOther = fromKo + honbaEach;
			best.points        = best.fromDealer + best.fromEachOther * ( payers - 1 ) + sticks;
		}

		return best;
	}

	bool HasYaku( const Counts34 &concealed, const Meld *melds, const uint8_t meldCount,
		const WinContext &context )
	{
		return ScoreHand( concealed, melds, meldCount, context ).valid;
	}

	WinContext MakeWinContext( const GameState &state, const uint8_t winner )
	{
		WinContext ctx;

		if ( winner >= state.rules.numPlayers ) return ctx;

		const Player &p = state.players[winner];
		const HandResult &r = state.result;

		ctx.rules     = state.rules;
		ctx.byTsumo   = r.outcome == HandOutcome::TSUMO;
		ctx.seatWind  = p.seatWind;
		ctx.roundWind = state.roundWind;
		ctx.isDealer  = winner == state.dealer;

		ctx.winningTile = ctx.byTsumo ? p.drawn : r.winningTile;

		ctx.riichi       = p.riichiDeclared;
		ctx.doubleRiichi = p.doubleRiichi;
		ctx.ippatsu      = p.ippatsu;

		ctx.haitei  = r.haitei;
		ctx.houtei  = r.houtei;
		ctx.rinshan = r.rinshan;
		ctx.chankan = r.chankan;

		for ( uint8_t i = 0; i < state.doraIndicators && i < MAX_DORA_INDICATORS; ++i )
		{
			ctx.doraIndicators[ctx.doraIndicatorCount++] = state.DoraIndicator( i );
			ctx.uraIndicators[ctx.uraIndicatorCount++]   = state.UraIndicator( i );
		}

		// Red fives count wherever they sit.
		AkaMask aka = p.hand.Aka();
		for ( uint8_t i = 0; i < p.meldCount; ++i ) aka = static_cast<AkaMask>( aka | p.melds[i].aka );

		if ( InstanceIsAka( ctx.winningTile ) )
		{
			aka = static_cast<AkaMask>( aka | AkaBitFor( InstanceTile( ctx.winningTile ) ) );
		}

		uint8_t akaCount = 0;
		for ( int bit = 0; bit < 3; ++bit )
		{
			if ( ( aka & ( 1u << bit ) ) != 0 ) ++akaCount;
		}

		ctx.akaCount     = akaCount;
		ctx.nukiCount    = p.nukiCount;
		ctx.honba        = state.honba;
		ctx.riichiSticks = state.riichiSticks;

		return ctx;
	}

} // namespace LRMahjong::Model
