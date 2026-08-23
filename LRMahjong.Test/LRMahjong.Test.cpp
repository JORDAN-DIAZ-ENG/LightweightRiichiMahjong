#include "pch.h"
#include "CppUnitTest.h"

#include "LRMahjong.h"

#include <numeric>
#include <set>

using namespace LRMahjong;
using namespace LRMahjong::Model;

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace LRMahjongTest
{
	TEST_CLASS( DebugOnly )
	{
	public:
		TEST_METHOD( AlwaysReturnTrueIsExported )
		{
			Assert::IsTrue( AlwaysReturnTrue() );
		}
	};

	// -----------------------------------------------------------------
	// The canonical ordering is load bearing: a TileId is used directly as
	// an index into every 34-slot table, so any reordering silently
	// corrupts shanten, ukeire and yaku at once.
	// -----------------------------------------------------------------
	TEST_CLASS( TileOrdering )
	{
	public:
		TEST_METHOD( TileIdsAreContiguousFromZero )
		{
			Assert::AreEqual( 0,  static_cast<int>( Id( RiichiMahjongTile::MAN_1 ) ) );
			Assert::AreEqual( 8,  static_cast<int>( Id( RiichiMahjongTile::MAN_9 ) ) );
			Assert::AreEqual( 9,  static_cast<int>( Id( RiichiMahjongTile::PIN_1 ) ) );
			Assert::AreEqual( 17, static_cast<int>( Id( RiichiMahjongTile::PIN_9 ) ) );
			Assert::AreEqual( 18, static_cast<int>( Id( RiichiMahjongTile::SOU_1 ) ) );
			Assert::AreEqual( 26, static_cast<int>( Id( RiichiMahjongTile::SOU_9 ) ) );
			Assert::AreEqual( 27, static_cast<int>( Id( RiichiMahjongTile::EAST ) ) );
			Assert::AreEqual( 30, static_cast<int>( Id( RiichiMahjongTile::NORTH ) ) );
			Assert::AreEqual( 34, static_cast<int>( Id( RiichiMahjongTile::TILE_COUNT ) ) );
		}

		TEST_METHOD( DragonsAreInCanonicalHakuHatsuChunOrder )
		{
			// Tenhou z ordering. Getting this wrong was the pre-existing bug:
			// dragons used to be declared red, white, green.
			Assert::AreEqual( 31, static_cast<int>( Id( RiichiMahjongTile::WHITE_DRAGON ) ) );
			Assert::AreEqual( 32, static_cast<int>( Id( RiichiMahjongTile::GREEN_DRAGON ) ) );
			Assert::AreEqual( 33, static_cast<int>( Id( RiichiMahjongTile::RED_DRAGON ) ) );

			Assert::AreEqual( std::string( "5z" ), TileToTenhouString( Id( RiichiMahjongTile::WHITE_DRAGON ) ) );
			Assert::AreEqual( std::string( "6z" ), TileToTenhouString( Id( RiichiMahjongTile::GREEN_DRAGON ) ) );
			Assert::AreEqual( std::string( "7z" ), TileToTenhouString( Id( RiichiMahjongTile::RED_DRAGON ) ) );
		}
	};

	TEST_CLASS( TilePredicates )
	{
	public:
		TEST_METHOD( ExactlyOneCategoryPerTile )
		{
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				const int categories =
					( IsManzu( t ) ? 1 : 0 ) +
					( IsPinzu( t ) ? 1 : 0 ) +
					( IsSouzu( t ) ? 1 : 0 ) +
					( IsHonor( t ) ? 1 : 0 );

				Assert::AreEqual( 1, categories, L"every tile belongs to exactly one category" );
			}
		}

		TEST_METHOD( WindsAndDragonsPartitionTheHonours )
		{
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				if ( IsHonor( t ) )
				{
					Assert::IsTrue( IsWind( t ) != IsDragon( t ) );
					Assert::IsFalse( IsSuited( t ) );
				}
				else
				{
					Assert::IsFalse( IsWind( t ) );
					Assert::IsFalse( IsDragon( t ) );
					Assert::IsTrue( IsSuited( t ) );
				}
			}
		}

		TEST_METHOD( RankAndSuitAgreeWithTheIndex )
		{
			for ( TileId t = 0; t < 27; ++t )
			{
				Assert::AreEqual( static_cast<int>( t % 9 + 1 ), static_cast<int>( RankOf( t ) ) );
				Assert::AreEqual( static_cast<int>( t / 9 ), static_cast<int>( SuitOf( t ) ) );
			}

			for ( TileId t = 27; t < TILE_KIND_COUNT; ++t )
			{
				Assert::AreEqual( 0, static_cast<int>( RankOf( t ) ) );
				Assert::IsTrue( SuitOf( t ) == Suit::HONOR );
			}
		}

		TEST_METHOD( TerminalsAreOnesAndNines )
		{
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				const bool expected = IsSuited( t ) && ( RankOf( t ) == 1 || RankOf( t ) == 9 );
				Assert::AreEqual( expected, IsTerminal( t ) );
				Assert::AreEqual( IsTerminal( t ) || IsHonor( t ), IsTerminalOrHonor( t ) );
				Assert::AreEqual( IsSuited( t ) && !IsTerminal( t ), IsSimple( t ) );
			}
		}

		TEST_METHOD( SanmaRemovesExactlyManTwoThroughEight )
		{
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				const bool expected = !( t >= Id( RiichiMahjongTile::MAN_2 ) && t <= Id( RiichiMahjongTile::MAN_8 ) );
				Assert::AreEqual( expected, IsSanmaTile( t ) );
			}
		}
	};

	// -----------------------------------------------------------------
	// Round trips. Exhaustive over all 34 kinds, per the M0 deliverable.
	// -----------------------------------------------------------------
	TEST_CLASS( TenhouRoundTrip )
	{
	public:
		TEST_METHOD( EveryTileRoundTripsThroughTenhouNotation )
		{
			std::set<std::string> seen;

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				const std::string text = TileToTenhouString( t );
				Assert::AreEqual( size_t( 2 ), text.size() );

				// Notation must be unique, or parsing is ambiguous.
				Assert::IsTrue( seen.insert( text ).second, L"duplicate tenhou notation" );

				Assert::AreEqual( static_cast<int>( t ), static_cast<int>( TileFromTenhouString( text ) ) );
			}

			Assert::AreEqual( size_t( TILE_KIND_COUNT ), seen.size() );
		}

		TEST_METHOD( MalformedTenhouTilesAreRejected )
		{
			Assert::AreEqual( static_cast<int>( INVALID_TILE ), static_cast<int>( TileFromTenhouString( "" ) ) );
			Assert::AreEqual( static_cast<int>( INVALID_TILE ), static_cast<int>( TileFromTenhouString( "1" ) ) );
			Assert::AreEqual( static_cast<int>( INVALID_TILE ), static_cast<int>( TileFromTenhouString( "1q" ) ) );
			Assert::AreEqual( static_cast<int>( INVALID_TILE ), static_cast<int>( TileFromTenhouString( "8z" ) ) );
			Assert::AreEqual( static_cast<int>( INVALID_TILE ), static_cast<int>( TileFromTenhouString( "0z" ) ) );
			Assert::AreEqual( static_cast<int>( INVALID_TILE ), static_cast<int>( TileFromTenhouString( "123m" ) ) );
		}

		TEST_METHOD( RedFiveParsesAsTheOrdinaryFive )
		{
			Assert::AreEqual( static_cast<int>( MAN_5_ID ), static_cast<int>( TileFromTenhouString( "0m" ) ) );
			Assert::AreEqual( static_cast<int>( PIN_5_ID ), static_cast<int>( TileFromTenhouString( "0p" ) ) );
			Assert::AreEqual( static_cast<int>( SOU_5_ID ), static_cast<int>( TileFromTenhouString( "0s" ) ) );
		}

		TEST_METHOD( HandsRoundTripThroughTenhouStrings )
		{
			const char *hands[] = {
				"123m456p789s11z",
				"1112345678999m",
				"19m19p19s1234567z",
				"222m333p444s55z",
				"11223344556677z",
			};

			for ( const char *text : hands )
			{
				Hand hand;
				Assert::IsTrue( Hand::FromTenhouString( text, hand ), L"should parse" );
				Assert::AreEqual( std::string( text ), hand.ToTenhouString() );
			}
		}

		TEST_METHOD( RedFivesSurviveTheRoundTrip )
		{
			Hand hand;
			Assert::IsTrue( Hand::FromTenhouString( "0m0p0s", hand ) );
			Assert::AreEqual( static_cast<int>( AKA_MAN5 | AKA_PIN5 | AKA_SOU5 ), static_cast<int>( hand.Aka() ) );
			Assert::AreEqual( 3, static_cast<int>( hand.TotalTiles() ) );
			Assert::AreEqual( std::string( "0m0p0s" ), hand.ToTenhouString() );

			// A red five alongside ordinary fives: the red one is written first.
			Hand mixed;
			Assert::IsTrue( Hand::FromTenhouString( "055p", mixed ) );
			Assert::AreEqual( 3, static_cast<int>( mixed.Count( PIN_5_ID ) ) );
			Assert::AreEqual( std::string( "055p" ), mixed.ToTenhouString() );
		}

		TEST_METHOD( MalformedHandsAreRejected )
		{
			Hand hand;
			Assert::IsFalse( Hand::FromTenhouString( "123", hand ),    L"no suit character" );
			Assert::IsFalse( Hand::FromTenhouString( "m123", hand ),   L"suit before digits" );
			Assert::IsFalse( Hand::FromTenhouString( "11111m", hand ), L"fifth copy" );
			Assert::IsFalse( Hand::FromTenhouString( "8z", hand ),     L"no eighth honour" );
			Assert::IsFalse( Hand::FromTenhouString( "00p", hand ),    L"two of the same red five" );
			Assert::IsFalse( Hand::FromTenhouString( "111222333m4444p55s", hand ), L"more than fourteen tiles" );
		}
	};

	TEST_CLASS( HandOperations )
	{
	public:
		TEST_METHOD( AddAndRemoveTrackTheTotal )
		{
			Hand hand;
			Assert::AreEqual( 0, static_cast<int>( hand.TotalTiles() ) );

			for ( int i = 0; i < 4; ++i )
			{
				Assert::IsTrue( hand.Add( 0 ) );
			}
			Assert::AreEqual( 4, static_cast<int>( hand.Count( 0 ) ) );
			Assert::AreEqual( 4, static_cast<int>( hand.TotalTiles() ) );

			Assert::IsFalse( hand.Add( 0 ), L"fifth copy must be refused" );
			Assert::AreEqual( 4, static_cast<int>( hand.TotalTiles() ) );

			Assert::IsTrue( hand.Remove( 0 ) );
			Assert::AreEqual( 3, static_cast<int>( hand.TotalTiles() ) );

			Assert::IsFalse( hand.Remove( 1 ), L"removing an absent tile must fail" );
		}

		TEST_METHOD( RedFiveBookkeeping )
		{
			Hand hand;
			Assert::IsTrue( hand.Add( PIN_5_ID, true ) );
			Assert::AreEqual( static_cast<int>( AKA_PIN5 ), static_cast<int>( hand.Aka() ) );

			Assert::IsFalse( hand.Add( PIN_5_ID, true ), L"only one red 5p exists" );
			Assert::IsTrue( hand.Add( PIN_5_ID ), L"an ordinary 5p is still fine" );

			Assert::IsFalse( hand.Add( 0, true ), L"1m has no red variant" );

			Assert::IsTrue( hand.Remove( PIN_5_ID, true ) );
			Assert::AreEqual( static_cast<int>( AKA_NONE ), static_cast<int>( hand.Aka() ) );
		}

		TEST_METHOD( InvalidTilesAreRefused )
		{
			Hand hand;
			Assert::IsFalse( hand.Add( TILE_KIND_COUNT ) );
			Assert::IsFalse( hand.Add( INVALID_TILE ) );
			Assert::AreEqual( 0, static_cast<int>( hand.TotalTiles() ) );
		}
	};

	TEST_CLASS( RuleTables )
	{
	public:
		TEST_METHOD( FourPlayerWallIs136 )
		{
			const Rules rules = MahjongSoul4P();
			Assert::AreEqual( 136, static_cast<int>( rules.TotalTiles() ) );

			int total = 0;
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) total += rules.CopiesOf( t );
			Assert::AreEqual( 136, total, L"copies must sum to the wall size" );

			// 136 minus a 14 tile dead wall minus 52 dealt.
			Assert::AreEqual( 70, static_cast<int>( rules.LiveWallTiles() ) );
			Assert::IsTrue( rules.allowChi );
			Assert::IsFalse( rules.nukidora );
		}

		TEST_METHOD( ThreePlayerWallIs108 )
		{
			const Rules rules = MahjongSoul3P();
			Assert::AreEqual( 108, static_cast<int>( rules.TotalTiles() ) );

			int total = 0;
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) total += rules.CopiesOf( t );
			Assert::AreEqual( 108, total, L"copies must sum to the wall size" );

			// 108 minus a 14 tile dead wall minus 39 dealt.
			Assert::AreEqual( 55, static_cast<int>( rules.LiveWallTiles() ) );
			Assert::IsFalse( rules.allowChi );
			Assert::IsTrue( rules.nukidora );
			Assert::AreEqual( 35000, rules.startingPoints );
		}

		TEST_METHOD( SanmaHasNoMiddleManzu )
		{
			const Rules rules = MahjongSoul3P();

			for ( TileId t = Id( RiichiMahjongTile::MAN_2 ); t <= Id( RiichiMahjongTile::MAN_8 ); ++t )
			{
				Assert::AreEqual( 0, static_cast<int>( rules.CopiesOf( t ) ) );
			}

			Assert::AreEqual( 4, static_cast<int>( rules.CopiesOf( Id( RiichiMahjongTile::MAN_1 ) ) ) );
			Assert::AreEqual( 4, static_cast<int>( rules.CopiesOf( Id( RiichiMahjongTile::MAN_9 ) ) ) );
		}
	};

	// -----------------------------------------------------------------
	// If either of these regresses, a rollout has stopped being a memcpy
	// and the whole search budget has silently changed.
	// -----------------------------------------------------------------
	TEST_CLASS( StateLayout )
	{
	public:
		TEST_METHOD( GameStateIsTriviallyCopyable )
		{
			Assert::IsTrue( std::is_trivially_copyable_v<GameState> );
			Assert::IsTrue( std::is_trivially_copyable_v<Player> );
			Assert::IsTrue( std::is_trivially_copyable_v<Hand> );
			Assert::IsTrue( std::is_trivially_copyable_v<Meld> );
		}

		TEST_METHOD( GameStateStaysWithinItsBudget )
		{
			Assert::IsTrue( sizeof( GameState ) <= 1024,
				L"GameState must stay under 1 KB for rollouts to stay cheap" );
		}

		TEST_METHOD( ResetSeatsWindsAndPoints )
		{
			GameState state;
			state.Reset( MahjongSoul4P(), 0 );

			Assert::AreEqual( 136, static_cast<int>( state.wallCount ) );
			Assert::AreEqual( 122, static_cast<int>( state.liveWallTail ) );

			for ( uint8_t seat = 0; seat < 4; ++seat )
			{
				Assert::AreEqual( 25000, state.players[seat].points );
				Assert::AreEqual( static_cast<int>( seat ), static_cast<int>( state.players[seat].seatWind ) );
			}

			// With a non-zero dealer the winds rotate with the seat.
			GameState shifted;
			shifted.Reset( MahjongSoul4P(), 2 );
			Assert::AreEqual( 0, static_cast<int>( shifted.players[2].seatWind ), L"the dealer is east" );
			Assert::AreEqual( 1, static_cast<int>( shifted.players[3].seatWind ) );
			Assert::AreEqual( 2, static_cast<int>( shifted.players[0].seatWind ) );
			Assert::AreEqual( 3, static_cast<int>( shifted.players[1].seatWind ) );
		}

		TEST_METHOD( SanmaResetUsesThreeSeats )
		{
			GameState state;
			state.Reset( MahjongSoul3P(), 0 );

			Assert::AreEqual( 108, static_cast<int>( state.wallCount ) );
			Assert::AreEqual( 94,  static_cast<int>( state.liveWallTail ) );
			Assert::AreEqual( 3,   static_cast<int>( state.PlayerCount() ) );

			for ( uint8_t seat = 0; seat < 3; ++seat )
			{
				Assert::AreEqual( 35000, state.players[seat].points );
			}
		}
	};

	TEST_CLASS( MeldBehaviour )
	{
	public:
		TEST_METHOD( MeldsExpandIntoAHistogram )
		{
			Counts34 counts{};

			Meld chi;
			chi.type = MeldType::CHI;
			chi.base = 0; // 1m 2m 3m
			chi.AddTo( counts );

			Assert::AreEqual( 1, static_cast<int>( counts[0] ) );
			Assert::AreEqual( 1, static_cast<int>( counts[1] ) );
			Assert::AreEqual( 1, static_cast<int>( counts[2] ) );

			Meld pon;
			pon.type = MeldType::PON;
			pon.base = 9;
			pon.AddTo( counts );
			Assert::AreEqual( 3, static_cast<int>( counts[9] ) );

			Meld kan;
			kan.type = MeldType::ANKAN;
			kan.base = 18;
			kan.AddTo( counts );
			Assert::AreEqual( 4, static_cast<int>( counts[18] ) );
			Assert::AreEqual( 4, static_cast<int>( kan.TileCount() ) );
			Assert::IsTrue( kan.IsKan() );
			Assert::IsTrue( kan.IsConcealed() );
		}

		TEST_METHOD( OnlyAnkanKeepsAHandClosed )
		{
			Player player;
			Assert::IsTrue( player.IsMenzen(), L"a hand with no melds is closed" );
			Assert::AreEqual( 13, static_cast<int>( player.ExpectedHandSize() ) );

			player.melds[0].type = MeldType::ANKAN;
			player.melds[0].base = 0;
			player.meldCount = 1;
			Assert::IsTrue( player.IsMenzen(), L"a concealed kan keeps the hand closed" );
			Assert::AreEqual( 10, static_cast<int>( player.ExpectedHandSize() ) );

			player.melds[0].type = MeldType::PON;
			Assert::IsFalse( player.IsMenzen() );

			player.drawn = MakeInstance( 5 );
			Assert::AreEqual( 11, static_cast<int>( player.ExpectedHandSize() ), L"holding a draw adds one" );
		}
	};

	TEST_CLASS( VisibleTileCounting )
	{
	public:
		TEST_METHOD( DiscardsMeldsAndNukiAreAllVisible )
		{
			GameState state;
			state.Reset( MahjongSoul3P(), 0 );

			state.players[0].hand.Add( 0 );                    // 1m in my hand
			state.players[1].discards[0]   = MakeInstance( 9 ); // 1p discarded
			state.players[1].discardCount  = 1;
			state.players[2].melds[0].type = MeldType::PON;
			state.players[2].melds[0].base = 18;               // pon of 1s
			state.players[2].meldCount     = 1;
			state.players[2].nukiCount     = 2;                // two Norths pulled

			const Counts34 visible = state.VisibleTo( 0 );

			Assert::AreEqual( 1, static_cast<int>( visible[0] ) );
			Assert::AreEqual( 1, static_cast<int>( visible[9] ) );
			Assert::AreEqual( 3, static_cast<int>( visible[18] ) );
			Assert::AreEqual( 2, static_cast<int>( visible[Id( RiichiMahjongTile::NORTH )] ) );
		}

		TEST_METHOD( ACalledDiscardIsNotCountedTwice )
		{
			GameState state;
			state.Reset( MahjongSoul4P(), 0 );

			// Seat 1 discarded 1s and seat 2 ponned it: the tile now lives in
			// the meld, so the pond must not count it again.
			state.players[1].discards[0]     = MakeInstance( 18 );
			state.players[1].discardFlags[0] = DISCARD_CALLED;
			state.players[1].discardCount    = 1;

			state.players[2].melds[0].type = MeldType::PON;
			state.players[2].melds[0].base = 18;
			state.players[2].meldCount     = 1;

			const Counts34 visible = state.VisibleTo( 0 );
			Assert::AreEqual( 3, static_cast<int>( visible[18] ), L"three copies, not four" );
		}
	};

	// -----------------------------------------------------------------
	// Reproducibility is what makes a rollout debuggable; without it a bad
	// result cannot be replayed.
	// -----------------------------------------------------------------
	TEST_CLASS( RandomNumbers )
	{
	public:
		TEST_METHOD( SameSeedGivesTheSameStream )
		{
			Rng a( 42 );
			Rng b( 42 );

			for ( int i = 0; i < 10000; ++i )
			{
				Assert::AreEqual( a.NextU64(), b.NextU64() );
			}
		}

		TEST_METHOD( DifferentSeedsDiverge )
		{
			Rng a( 1 );
			Rng b( 2 );

			bool diverged = false;
			for ( int i = 0; i < 100 && !diverged; ++i )
			{
				if ( a.NextU64() != b.NextU64() ) diverged = true;
			}
			Assert::IsTrue( diverged );
		}

		TEST_METHOD( BelowStaysInRange )
		{
			Rng rng( 7 );
			for ( int i = 0; i < 100000; ++i )
			{
				Assert::IsTrue( rng.Below( 34 ) < 34u );
			}
			Assert::AreEqual( 0u, rng.Below( 0 ), L"a zero bound must not divide by zero" );
		}

		TEST_METHOD( IntIsInclusiveAtBothEnds )
		{
			Rng rng( 99 );
			bool sawMin = false;
			bool sawMax = false;

			for ( int i = 0; i < 10000; ++i )
			{
				const int v = rng.Int( 3, 6 );
				Assert::IsTrue( v >= 3 && v <= 6 );
				if ( v == 3 ) sawMin = true;
				if ( v == 6 ) sawMax = true;
			}

			Assert::IsTrue( sawMin && sawMax );
			Assert::AreEqual( 5, rng.Int( 5, 5 ), L"a degenerate range returns its bound" );
		}

		TEST_METHOD( ShuffleIsAPermutation )
		{
			std::array<int, 136> tiles;
			std::iota( tiles.begin(), tiles.end(), 0 );

			Rng rng( 2024 );
			rng.Shuffle( tiles.begin(), tiles.end() );

			std::array<bool, 136> seen{};
			for ( const int t : tiles )
			{
				Assert::IsTrue( t >= 0 && t < 136 );
				Assert::IsFalse( seen[t], L"shuffle must not duplicate an element" );
				seen[t] = true;
			}
		}

		TEST_METHOD( StateCanBeSavedAndRestored )
		{
			Rng rng( 123 );
			for ( int i = 0; i < 50; ++i ) rng.NextU64();

			uint64_t saved[4];
			rng.GetState( saved );

			const uint64_t expected = rng.NextU64();

			rng.SetState( saved );
			Assert::AreEqual( expected, rng.NextU64() );
		}
	};

	TEST_CLASS( EngineSetup )
	{
	public:
		TEST_METHOD( EngineIsSeededAndReproducible )
		{
			Engine a( MahjongSoul4P(), 555 );
			Engine b( MahjongSoul4P(), 555 );

			for ( int i = 0; i < 1000; ++i )
			{
				Assert::AreEqual( a.Random().NextU64(), b.Random().NextU64() );
			}
		}

		TEST_METHOD( ResetReplaysTheSameStream )
		{
			Engine engine( MahjongSoul4P(), 0 );
			engine.Reset( 777 );
			const uint64_t first = engine.Random().NextU64();

			engine.Reset( 777 );
			Assert::AreEqual( first, engine.Random().NextU64() );
		}

		TEST_METHOD( EngineCarriesItsRules )
		{
			Engine sanma( MahjongSoul3P(), 1 );
			Assert::AreEqual( 3, static_cast<int>( sanma.GetRules().numPlayers ) );
			Assert::AreEqual( 108, static_cast<int>( sanma.State().wallCount ) );
		}
	};
}
