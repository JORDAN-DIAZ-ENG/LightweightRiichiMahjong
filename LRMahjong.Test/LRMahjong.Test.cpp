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

			// A call leaves the seat owing a discard without setting `drawn`,
			// so the extra tile is tracked by awaitingDiscard rather than by
			// the drawn tile.
			player.awaitingDiscard = true;
			Assert::AreEqual( 11, static_cast<int>( player.ExpectedHandSize() ), L"owing a discard adds one" );
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

	// =================================================================
	// M1: wall, deal and the turn state machine.
	// =================================================================

	namespace
	{
		// Every tile, wherever it currently is. The dead wall always holds 14
		// physical tiles: each replacement draw takes one out and pulls the
		// live wall's last tile in behind it.
		int TilesInPlay( const GameState &s )
		{
			int held = 0;

			for ( uint8_t seat = 0; seat < s.PlayerCount(); ++seat )
			{
				const Player &p = s.players[seat];

				held += p.hand.TotalTiles();

				for ( uint8_t i = 0; i < p.meldCount; ++i ) held += p.melds[i].TileCount();

				// A called discard now lives in the caller's meld.
				for ( uint8_t i = 0; i < p.discardCount; ++i )
				{
					if ( ( p.discardFlags[i] & DISCARD_CALLED ) == 0 ) ++held;
				}

				held += p.nukiCount;
			}

			return held + s.LiveWallRemaining() + DEAD_WALL_TILES;
		}

		// Discards whatever was just drawn. Always legal, so it drives a hand
		// to its natural end without needing the legality checks that arrive
		// in M2.
		void PlayOutTsumogiri( Engine &engine, const int maxSteps = 4000 )
		{
			int steps = 0;

			while ( engine.State().phase != Phase::HAND_OVER && steps < maxSteps )
			{
				const GameState &s = engine.State();
				++steps;

				if ( s.phase == Phase::DISCARD )
				{
					const uint8_t seat = s.currentPlayer;
					engine.Step( Action::Discard( seat, s.players[seat].drawn ) );
				}
				else if ( s.phase == Phase::CALL )
				{
					for ( uint8_t seat = 0; seat < s.PlayerCount(); ++seat )
					{
						if ( ( s.pendingCallers & ( 1u << seat ) ) != 0 )
						{
							engine.Step( Action::Pass( seat ) );
							break;
						}
					}
				}
			}
		}

		// Replaces a seat's concealed hand outright. Test scenarios need exact
		// shapes, and going through the tenhou parser keeps them readable.
		void SetHand( Player &p, const char *tenhou )
		{
			Hand hand;
			Assert::IsTrue( Hand::FromTenhouString( tenhou, hand ), L"test hand should parse" );
			p.hand = hand;
		}

		// 123m 456m 789m 123p + 1p1p. A complete standard hand.
		constexpr const char *WINNING_14 = "123456789m11123p";

		// The same hand one tile short, waiting on 1p and 4p.
		constexpr const char *TENPAI_13 = "123456789m1123p";

		// Answers every outstanding call response with a pass.
		void PassEveryone( Engine &engine )
		{
			const GameState &s = engine.State();
			for ( uint8_t seat = 0; seat < s.PlayerCount(); ++seat )
			{
				if ( ( s.pendingCallers & ( 1u << seat ) ) != 0 ) engine.Step( Action::Pass( seat ) );
			}
		}

		int TotalDiscards( const GameState &s )
		{
			int total = 0;
			for ( uint8_t seat = 0; seat < s.PlayerCount(); ++seat ) total += s.players[seat].discardCount;
			return total;
		}
	}

	TEST_CLASS( WallConstruction )
	{
	public:
		TEST_METHOD( FourPlayerWallHoldsFourOfEveryKind )
		{
			Engine engine( MahjongSoul4P(), 1 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();
			Assert::AreEqual( 136, static_cast<int>( s.wallCount ) );

			Counts34 counts{};
			for ( uint8_t i = 0; i < s.wallCount; ++i ) ++counts[InstanceTile( s.wall[i] )];

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				Assert::AreEqual( 4, static_cast<int>( counts[t] ) );
			}
		}

		TEST_METHOD( ThreePlayerWallSkipsTheMiddleManzu )
		{
			Engine engine( MahjongSoul3P(), 1 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();
			Assert::AreEqual( 108, static_cast<int>( s.wallCount ) );

			Counts34 counts{};
			for ( uint8_t i = 0; i < s.wallCount; ++i ) ++counts[InstanceTile( s.wall[i] )];

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				const int expected = IsSanmaTile( t ) ? 4 : 0;
				Assert::AreEqual( expected, static_cast<int>( counts[t] ) );
			}
		}

		TEST_METHOD( ExactlyOneRedFivePerSuit )
		{
			Engine engine( MahjongSoul4P(), 9 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();

			int reds[3] = { 0, 0, 0 };
			for ( uint8_t i = 0; i < s.wallCount; ++i )
			{
				if ( !InstanceIsAka( s.wall[i] ) ) continue;

				const TileId t = InstanceTile( s.wall[i] );
				Assert::AreEqual( 5, static_cast<int>( RankOf( t ) ), L"only fives are red" );
				++reds[static_cast<int>( SuitOf( t ) )];
			}

			Assert::AreEqual( 1, reds[0] );
			Assert::AreEqual( 1, reds[1] );
			Assert::AreEqual( 1, reds[2] );
		}

		TEST_METHOD( SanmaHasNoRedManzu )
		{
			Engine engine( MahjongSoul3P(), 9 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();
			for ( uint8_t i = 0; i < s.wallCount; ++i )
			{
				if ( InstanceIsAka( s.wall[i] ) )
				{
					Assert::IsFalse( IsManzu( InstanceTile( s.wall[i] ) ), L"5m does not exist in sanma" );
				}
			}
		}

		TEST_METHOD( SameSeedBuildsTheSameWall )
		{
			Engine a( MahjongSoul4P(), 31337 );
			Engine b( MahjongSoul4P(), 31337 );
			a.StartHand( 0 );
			b.StartHand( 0 );

			for ( uint8_t i = 0; i < a.State().wallCount; ++i )
			{
				Assert::AreEqual( static_cast<int>( a.State().wall[i] ), static_cast<int>( b.State().wall[i] ) );
			}
		}

		TEST_METHOD( DifferentSeedsBuildDifferentWalls )
		{
			Engine a( MahjongSoul4P(), 1 );
			Engine b( MahjongSoul4P(), 2 );
			a.StartHand( 0 );
			b.StartHand( 0 );

			bool differs = false;
			for ( uint8_t i = 0; i < a.State().wallCount && !differs; ++i )
			{
				if ( a.State().wall[i] != b.State().wall[i] ) differs = true;
			}
			Assert::IsTrue( differs );
		}
	};

	TEST_CLASS( Dealing )
	{
	public:
		TEST_METHOD( EveryoneGetsThirteenAndTheDealerFourteen )
		{
			Engine engine( MahjongSoul4P(), 77 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();

			Assert::AreEqual( 14, static_cast<int>( s.players[0].hand.TotalTiles() ) );
			Assert::IsTrue( s.players[0].awaitingDiscard );
			Assert::IsTrue( IsValidInstance( s.players[0].drawn ) );

			for ( uint8_t seat = 1; seat < 4; ++seat )
			{
				Assert::AreEqual( 13, static_cast<int>( s.players[seat].hand.TotalTiles() ) );
				Assert::IsFalse( s.players[seat].awaitingDiscard );
			}

			Assert::IsTrue( s.phase == Phase::DISCARD );
			Assert::AreEqual( 0, static_cast<int>( s.currentPlayer ) );
		}

		TEST_METHOD( SanmaDealsThreeHands )
		{
			Engine engine( MahjongSoul3P(), 77 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();
			Assert::AreEqual( 14, static_cast<int>( s.players[0].hand.TotalTiles() ) );
			Assert::AreEqual( 13, static_cast<int>( s.players[1].hand.TotalTiles() ) );
			Assert::AreEqual( 13, static_cast<int>( s.players[2].hand.TotalTiles() ) );
			Assert::AreEqual( 0,  static_cast<int>( s.players[3].hand.TotalTiles() ) );
		}

		TEST_METHOD( TheFirstDoraIndicatorIsFaceUp )
		{
			Engine engine( MahjongSoul4P(), 5 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();
			Assert::AreEqual( 1, static_cast<int>( s.doraIndicators ) );
			Assert::IsTrue( IsValidInstance( s.DoraIndicator( 0 ) ) );
			Assert::IsFalse( IsValidInstance( s.DoraIndicator( 1 ) ), L"only one is turned at the start" );
		}

		TEST_METHOD( WallAccountingAfterTheDeal )
		{
			Engine engine( MahjongSoul4P(), 5 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();

			// 52 dealt plus the dealer's fourteenth.
			Assert::AreEqual( 53, static_cast<int>( s.liveWallHead ) );
			Assert::AreEqual( 122, static_cast<int>( s.liveWallTail ) );
			Assert::AreEqual( 69, static_cast<int>( s.LiveWallRemaining() ) );
			Assert::AreEqual( 136, TilesInPlay( s ) );
		}
	};

	TEST_CLASS( TurnMachine )
	{
	public:
		// The window only opens for seats that actually have a choice. A seat
		// whose only legal answer is "pass" is answered for by the engine, the
		// same way a seat whose turn arrives is dealt its tile.
		TEST_METHOD( TheCallWindowOnlyOpensForSeatsWithAChoice )
		{
			Engine engine( MahjongSoul4P(), 11 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId thrown = Id( RiichiMahjongTile::SOU_7 );
			SetHand( s.players[0], "7s" );
			s.players[0].drawn = MakeInstance( thrown );

			// Only seat 2 can do anything with it.
			SetHand( s.players[1], "1m" );
			SetHand( s.players[2], "779s" );
			SetHand( s.players[3], "1m" );

			const StepResult r = engine.Step( Action::Discard( 0, MakeInstance( thrown ) ) );

			Assert::IsTrue( r == StepResult::OK );
			Assert::IsTrue( s.phase == Phase::CALL );
			Assert::AreEqual( 0b0100, static_cast<int>( s.pendingCallers ), L"only the seat that can pon" );
			Assert::AreEqual( 1, static_cast<int>( s.players[0].discardCount ) );
			Assert::IsFalse( s.players[0].awaitingDiscard );
			Assert::AreEqual( 0, static_cast<int>( s.lastDiscarder ) );
		}

		TEST_METHOD( PassingResolvesTheDiscardAndAdvancesTheTurn )
		{
			Engine engine( MahjongSoul4P(), 11 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId thrown = Id( RiichiMahjongTile::SOU_7 );
			SetHand( s.players[0], "7s" );
			s.players[0].drawn = MakeInstance( thrown );
			SetHand( s.players[1], "1m" );
			SetHand( s.players[2], "779s" );
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( thrown ) ) );
			Assert::IsTrue( s.phase == Phase::CALL );

			engine.Step( Action::Pass( 2 ) );

			Assert::IsTrue( s.phase == Phase::DISCARD );
			Assert::AreEqual( 1, static_cast<int>( s.currentPlayer ) );
			Assert::IsTrue( s.players[1].awaitingDiscard );
			Assert::IsTrue( IsValidInstance( s.players[1].drawn ) );
		}

		// With nobody able to call, the discard resolves inside the one step.
		TEST_METHOD( AnUninterestingDiscardResolvesImmediately )
		{
			Engine engine( MahjongSoul4P(), 11 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId thrown = Id( RiichiMahjongTile::SOU_7 );
			SetHand( s.players[0], "7s" );
			s.players[0].drawn = MakeInstance( thrown );
			for ( uint8_t seat = 1; seat < 4; ++seat ) SetHand( s.players[seat], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( thrown ) ) );

			Assert::IsTrue( s.phase == Phase::DISCARD );
			Assert::AreEqual( 0, static_cast<int>( s.pendingCallers ) );
			Assert::AreEqual( 1, static_cast<int>( s.currentPlayer ) );
		}

		TEST_METHOD( OutOfTurnAndWrongPhaseActionsAreRejected )
		{
			Engine engine( MahjongSoul4P(), 11 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();

			// Seat 2 is not the current player.
			Assert::IsTrue( engine.Step( Action::Discard( 2, s.players[2].drawn ) ) == StepResult::ILLEGAL );

			// Passing is meaningless while somebody owes a discard.
			Assert::IsTrue( engine.Step( Action::Pass( 1 ) ) == StepResult::ILLEGAL );

			// A seat outside the table.
			Assert::IsTrue( engine.Step( Action::Discard( 9, s.players[0].drawn ) ) == StepResult::ILLEGAL );

			// The state must be untouched by any of that.
			Assert::IsTrue( s.phase == Phase::DISCARD );
			Assert::AreEqual( 14, static_cast<int>( s.players[0].hand.TotalTiles() ) );
		}

		TEST_METHOD( DiscardingATileNotInHandIsRejected )
		{
			Engine engine( MahjongSoul4P(), 12 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();

			TileId missing = INVALID_TILE;
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				if ( s.players[0].hand.Count( t ) == 0 ) { missing = t; break; }
			}
			Assert::IsTrue( IsValidTile( missing ) );

			Assert::IsTrue( engine.Step( Action::Discard( 0, MakeInstance( missing ) ) ) == StepResult::ILLEGAL );
			Assert::AreEqual( 14, static_cast<int>( s.players[0].hand.TotalTiles() ) );
		}

		TEST_METHOD( NothingIsAcceptedOnceTheHandIsOver )
		{
			Engine engine( MahjongSoul4P(), 13 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			SetHand( s.players[0], WINNING_14 );
			s.players[0].drawn = MakeInstance( Id( RiichiMahjongTile::PIN_1 ) );

			Assert::IsTrue( engine.Step( Action::Tsumo( 0 ) ) == StepResult::HAND_ENDED );
			Assert::IsTrue( s.phase == Phase::HAND_OVER );

			Assert::IsTrue( engine.Step( Action::Pass( 1 ) ) == StepResult::ILLEGAL );
		}
	};

	TEST_CLASS( FullHand )
	{
	public:
		TEST_METHOD( FourPlayerHandExhaustsAfterSeventyDraws )
		{
			Engine engine( MahjongSoul4P(), 20250824 );
			engine.StartHand( 0 );
			PlayOutTsumogiri( engine );

			const GameState &s = engine.State();
			Assert::IsTrue( s.phase == Phase::HAND_OVER );
			Assert::IsTrue( s.result.outcome == HandOutcome::EXHAUSTIVE_DRAW );

			// 136 minus a 14 tile dead wall minus 52 dealt.
			Assert::AreEqual( 70, TotalDiscards( s ) );
			Assert::AreEqual( 0, static_cast<int>( s.LiveWallRemaining() ) );
		}

		TEST_METHOD( SanmaHandExhaustsAfterFiftyFiveDraws )
		{
			Engine engine( MahjongSoul3P(), 20250824 );
			engine.StartHand( 0 );
			PlayOutTsumogiri( engine );

			const GameState &s = engine.State();
			Assert::IsTrue( s.result.outcome == HandOutcome::EXHAUSTIVE_DRAW );

			// 108 minus a 14 tile dead wall minus 39 dealt.
			Assert::AreEqual( 55, TotalDiscards( s ) );
		}

		TEST_METHOD( TilesAreConservedAtEveryStep )
		{
			Engine engine( MahjongSoul4P(), 4242 );
			engine.StartHand( 0 );

			int guard = 0;
			while ( engine.State().phase != Phase::HAND_OVER && guard++ < 4000 )
			{
				Assert::AreEqual( 136, TilesInPlay( engine.State() ), L"a tile went missing" );

				const GameState &s = engine.State();
				if ( s.phase == Phase::DISCARD )
				{
					engine.Step( Action::Discard( s.currentPlayer, s.players[s.currentPlayer].drawn ) );
				}
				else
				{
					for ( uint8_t seat = 0; seat < s.PlayerCount(); ++seat )
					{
						if ( ( s.pendingCallers & ( 1u << seat ) ) != 0 )
						{
							engine.Step( Action::Pass( seat ) );
							break;
						}
					}
				}
			}

			Assert::AreEqual( 136, TilesInPlay( engine.State() ) );
		}

		TEST_METHOD( TheSameSeedReplaysTheSameHand )
		{
			Engine a( MahjongSoul4P(), 8080 );
			Engine b( MahjongSoul4P(), 8080 );
			a.StartHand( 0 );
			b.StartHand( 0 );
			PlayOutTsumogiri( a );
			PlayOutTsumogiri( b );

			for ( uint8_t seat = 0; seat < 4; ++seat )
			{
				Assert::AreEqual( static_cast<int>( a.State().players[seat].discardCount ),
					static_cast<int>( b.State().players[seat].discardCount ) );

				for ( uint8_t i = 0; i < a.State().players[seat].discardCount; ++i )
				{
					Assert::AreEqual( static_cast<int>( a.State().players[seat].discards[i] ),
						static_cast<int>( b.State().players[seat].discards[i] ) );
				}
			}
		}
	};

	TEST_CLASS( Calls )
	{
	public:
		TEST_METHOD( PonTakesTheTurnAndMarksTheDiscard )
		{
			Engine engine( MahjongSoul4P(), 606 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			// Hand sizes are deliberately not kept honest here; this exercises
			// the call mechanics, not the deal. The spare 9s matters: a call
			// that would leave nothing legal to discard is itself illegal.
			const TileId t = Id( RiichiMahjongTile::SOU_7 );
			const TileInstance thrown = MakeInstance( t );

			SetHand( s.players[0], "7s" );
			s.players[0].drawn = thrown;
			SetHand( s.players[1], "779s" );
			SetHand( s.players[2], "1m" );
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, thrown ) );
			Assert::IsTrue( engine.Step( Action::Pon( 1, t ) ) == StepResult::OK );

			Assert::AreEqual( 1, static_cast<int>( s.players[1].meldCount ) );
			Assert::IsTrue( s.players[1].melds[0].type == MeldType::PON );
			Assert::AreEqual( static_cast<int>( t ), static_cast<int>( s.players[1].melds[0].base ) );
			Assert::AreEqual( 1, static_cast<int>( s.players[1].hand.TotalTiles() ), L"both copies left the hand" );

			Assert::AreEqual( 1, static_cast<int>( s.currentPlayer ) );
			Assert::IsTrue( s.phase == Phase::DISCARD );
			Assert::IsTrue( s.players[1].awaitingDiscard );
			Assert::AreEqual( 0, static_cast<int>( s.pendingCallers ) );

			Assert::IsTrue( ( s.players[0].discardFlags[0] & DISCARD_CALLED ) != 0,
				L"the claimed tile must not be counted in the pond as well" );
			Assert::IsFalse( s.firstGoAround, L"any call ends the first go-around" );
		}

		TEST_METHOD( ChiOnlyFromTheLeftNeighbour )
		{
			Engine engine( MahjongSoul4P(), 707 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			// Force a known discard so the run is predictable.
			const TileId called = Id( RiichiMahjongTile::SOU_3 );
			s.players[0].hand.Clear();
			s.players[0].hand.Add( called );
			s.players[0].drawn = MakeInstance( called );
			s.players[0].awaitingDiscard = true;

			// The spare 9s matters: a chi that would leave the hand with no
			// legal discard is itself illegal.
			for ( uint8_t seat = 1; seat <= 2; ++seat ) SetHand( s.players[seat], "129s" );
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( called ) ) );

			// Seat 2 sits across, not to the discarder's right.
			Assert::IsTrue( engine.Step( Action::Chi( 2, Id( RiichiMahjongTile::SOU_1 ) ) ) == StepResult::ILLEGAL );

			Assert::IsTrue( engine.Step( Action::Chi( 1, Id( RiichiMahjongTile::SOU_1 ) ) ) == StepResult::OK );
			Assert::IsTrue( s.players[1].melds[0].type == MeldType::CHI );
			Assert::AreEqual( static_cast<int>( Id( RiichiMahjongTile::SOU_1 ) ),
				static_cast<int>( s.players[1].melds[0].base ) );
			Assert::AreEqual( 1, static_cast<int>( s.currentPlayer ) );
		}

		TEST_METHOD( AChiMustFormARealRun )
		{
			Engine engine( MahjongSoul4P(), 708 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId called = Id( RiichiMahjongTile::SOU_3 );
			s.players[0].hand.Clear();
			s.players[0].hand.Add( called );
			s.players[0].drawn = MakeInstance( called );
			s.players[0].awaitingDiscard = true;

			s.players[1].hand.Clear();
			s.players[1].hand.Add( Id( RiichiMahjongTile::SOU_1 ) );
			s.players[1].hand.Add( Id( RiichiMahjongTile::SOU_2 ) );

			engine.Step( Action::Discard( 0, MakeInstance( called ) ) );

			// A run that does not contain the discarded tile.
			Assert::IsTrue( engine.Step( Action::Chi( 1, Id( RiichiMahjongTile::SOU_5 ) ) ) == StepResult::ILLEGAL );

			// A run that would cross a suit boundary.
			Assert::IsTrue( engine.Step( Action::Chi( 1, Id( RiichiMahjongTile::PIN_8 ) ) ) == StepResult::ILLEGAL );

			// Honours can never form a run.
			Assert::IsTrue( engine.Step( Action::Chi( 1, Id( RiichiMahjongTile::EAST ) ) ) == StepResult::ILLEGAL );
		}

		TEST_METHOD( SanmaRefusesChi )
		{
			Engine engine( MahjongSoul3P(), 909 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId called = Id( RiichiMahjongTile::SOU_3 );
			s.players[0].hand.Clear();
			s.players[0].hand.Add( called );
			s.players[0].drawn = MakeInstance( called );
			s.players[0].awaitingDiscard = true;

			s.players[1].hand.Clear();
			s.players[1].hand.Add( Id( RiichiMahjongTile::SOU_1 ) );
			s.players[1].hand.Add( Id( RiichiMahjongTile::SOU_2 ) );

			engine.Step( Action::Discard( 0, MakeInstance( called ) ) );
			Assert::IsTrue( engine.Step( Action::Chi( 1, Id( RiichiMahjongTile::SOU_1 ) ) ) == StepResult::ILLEGAL );
		}
	};

	TEST_CLASS( Kans )
	{
	public:
		TEST_METHOD( AnkanTurnsItsIndicatorImmediately )
		{
			Engine engine( MahjongSoul4P(), 321 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId t = Id( RiichiMahjongTile::PIN_7 );
			s.players[0].hand.Clear();
			for ( int i = 0; i < 4; ++i ) s.players[0].hand.Add( t );
			s.players[0].awaitingDiscard = true;

			const int indicatorsBefore = s.doraIndicators;
			const int drawsBefore      = s.LiveWallRemaining();

			Assert::IsTrue( engine.Step( Action::Ankan( 0, t ) ) == StepResult::OK );

			Assert::AreEqual( 1, static_cast<int>( s.players[0].meldCount ) );
			Assert::IsTrue( s.players[0].melds[0].type == MeldType::ANKAN );
			Assert::IsTrue( s.players[0].melds[0].IsConcealed() );
			Assert::AreEqual( indicatorsBefore + 1, static_cast<int>( s.doraIndicators ) );
			Assert::IsFalse( s.pendingDoraFlip );

			// The replacement came from the dead wall and the live wall lost
			// its last tile to top it back up.
			Assert::AreEqual( 1, static_cast<int>( s.deadWallDraws ) );
			Assert::AreEqual( drawsBefore - 1, static_cast<int>( s.LiveWallRemaining() ) );
			Assert::AreEqual( 1, static_cast<int>( s.players[0].hand.TotalTiles() ), L"the replacement tile" );
			Assert::IsTrue( s.players[0].awaitingDiscard );
		}

		TEST_METHOD( ACalledKanDefersItsIndicatorUntilTheDiscard )
		{
			Engine engine( MahjongSoul4P(), 322 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			const TileInstance thrown = s.players[0].drawn;
			const TileId t = InstanceTile( thrown );

			s.players[1].hand.Clear();
			for ( int i = 0; i < 3; ++i ) s.players[1].hand.Add( t );

			const int indicatorsBefore = s.doraIndicators;

			engine.Step( Action::Discard( 0, thrown ) );
			Assert::IsTrue( engine.Step( Action::Daiminkan( 1, t ) ) == StepResult::OK );

			Assert::IsTrue( s.players[1].melds[0].type == MeldType::MINKAN );
			Assert::IsFalse( s.players[1].melds[0].IsConcealed() );
			Assert::AreEqual( indicatorsBefore, static_cast<int>( s.doraIndicators ), L"not yet" );
			Assert::IsTrue( s.pendingDoraFlip );

			engine.Step( Action::Discard( 1, s.players[1].drawn ) );
			Assert::AreEqual( indicatorsBefore + 1, static_cast<int>( s.doraIndicators ), L"now" );
			Assert::IsFalse( s.pendingDoraFlip );
		}

		TEST_METHOD( ShouminkanUpgradesAnExistingPon )
		{
			Engine engine( MahjongSoul4P(), 323 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId t = Id( RiichiMahjongTile::MAN_3 );
			s.players[0].hand.Clear();
			s.players[0].hand.Add( t );
			s.players[0].awaitingDiscard = true;
			s.players[0].melds[0].type = MeldType::PON;
			s.players[0].melds[0].base = t;
			s.players[0].meldCount = 1;

			Assert::IsTrue( engine.Step( Action::Shouminkan( 0, t ) ) == StepResult::OK );

			Assert::AreEqual( 1, static_cast<int>( s.players[0].meldCount ), L"upgraded, not appended" );
			Assert::IsTrue( s.players[0].melds[0].type == MeldType::SHOUMINKAN );
			Assert::IsTrue( s.players[0].melds[0].IsKan() );
			Assert::IsTrue( s.pendingDoraFlip );
		}

		TEST_METHOD( ShouminkanNeedsAMatchingPon )
		{
			Engine engine( MahjongSoul4P(), 324 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId t = Id( RiichiMahjongTile::MAN_3 );
			s.players[0].hand.Clear();
			s.players[0].hand.Add( t );
			s.players[0].awaitingDiscard = true;

			Assert::IsTrue( engine.Step( Action::Shouminkan( 0, t ) ) == StepResult::ILLEGAL );
		}

		TEST_METHOD( AnkanNeedsFourCopies )
		{
			Engine engine( MahjongSoul4P(), 325 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId t = Id( RiichiMahjongTile::PIN_7 );
			s.players[0].hand.Clear();
			for ( int i = 0; i < 3; ++i ) s.players[0].hand.Add( t );
			s.players[0].awaitingDiscard = true;

			Assert::IsTrue( engine.Step( Action::Ankan( 0, t ) ) == StepResult::ILLEGAL );
		}
	};

	TEST_CLASS( RiichiDeclaration )
	{
	public:
		TEST_METHOD( RiichiCostsAThousandAndAddsAStick )
		{
			Engine engine( MahjongSoul4P(), 44 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			const int pointsBefore = s.players[0].points;

			// Tenpai once the spare 9s goes.
			SetHand( s.players[0], "123456789m1123p9s" );
			const TileInstance spare = MakeInstance( Id( RiichiMahjongTile::SOU_9 ) );
			s.players[0].drawn = spare;

			Assert::IsTrue( engine.Step( Action::Riichi( 0, spare ) ) == StepResult::OK );

			Assert::IsTrue( s.players[0].riichiDeclared );
			Assert::IsTrue( s.players[0].ippatsu );
			Assert::IsTrue( s.players[0].doubleRiichi, L"declared on the first go-around" );
			Assert::AreEqual( pointsBefore - 1000, s.players[0].points );
			Assert::AreEqual( 1, static_cast<int>( s.riichiSticks ) );
			Assert::AreEqual( 0, static_cast<int>( s.players[0].riichiDiscardIndex ) );
			Assert::IsTrue( ( s.players[0].discardFlags[0] & DISCARD_RIICHI ) != 0 );
		}

		TEST_METHOD( AnOpenHandCannotDeclareRiichi )
		{
			Engine engine( MahjongSoul4P(), 45 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			s.players[0].melds[0].type = MeldType::PON;
			s.players[0].melds[0].base = 0;
			s.players[0].meldCount = 1;

			Assert::IsTrue( engine.Step( Action::Riichi( 0, s.players[0].drawn ) ) == StepResult::ILLEGAL );
		}

		TEST_METHOD( RiichiNeedsAThousandPoints )
		{
			Engine engine( MahjongSoul4P(), 46 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			s.players[0].points = 900;

			Assert::IsTrue( engine.Step( Action::Riichi( 0, s.players[0].drawn ) ) == StepResult::ILLEGAL );
		}

		TEST_METHOD( ADeclaredHandMayOnlyDiscardWhatItDrew )
		{
			Engine engine( MahjongSoul4P(), 47 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			SetHand( s.players[0], "123456789m1123p9s" );
			const TileInstance spare = MakeInstance( Id( RiichiMahjongTile::SOU_9 ) );
			s.players[0].drawn = spare;

			Assert::IsTrue( engine.Step( Action::Riichi( 0, spare ) ) == StepResult::OK );
			PassEveryone( engine );

			// Round back to the declarer.
			int guard = 0;
			while ( s.currentPlayer != 0 && s.phase != Phase::HAND_OVER && guard++ < 20 )
			{
				engine.Step( Action::Discard( s.currentPlayer, s.players[s.currentPlayer].drawn ) );
				PassEveryone( engine );
			}

			Assert::AreEqual( 0, static_cast<int>( s.currentPlayer ) );
			Assert::IsTrue( s.phase == Phase::DISCARD );

			// A declared hand is frozen: every discard on offer is the tile it
			// just drew.
			ActionList legal;
			LegalActions( s, 0, legal );

			const TileId drawnTile = InstanceTile( s.players[0].drawn );
			int discardOptions = 0;

			for ( const Action &a : legal )
			{
				if ( a.type != ActionType::DISCARD ) continue;
				++discardOptions;
				Assert::AreEqual( static_cast<int>( drawnTile ), static_cast<int>( InstanceTile( a.tile ) ),
					L"riichi may only shed what it drew" );
			}

			Assert::IsTrue( discardOptions > 0 );
			Assert::IsFalse( legal.ContainsType( ActionType::RIICHI ), L"already declared" );
		}
	};

	TEST_CLASS( SanmaKita )
	{
	public:
		TEST_METHOD( PullingANorthDrawsAReplacement )
		{
			Engine engine( MahjongSoul3P(), 55 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId north = Id( RiichiMahjongTile::NORTH );
			s.players[0].hand.Clear();
			s.players[0].hand.Add( north );
			s.players[0].awaitingDiscard = true;

			const int drawsBefore = s.LiveWallRemaining();

			Assert::IsTrue( engine.Step( Action::Kita( 0 ) ) == StepResult::OK );

			Assert::AreEqual( 1, static_cast<int>( s.players[0].nukiCount ) );
			Assert::AreEqual( 0, static_cast<int>( s.players[0].hand.Count( north ) ) );
			Assert::AreEqual( 1, static_cast<int>( s.deadWallDraws ) );
			Assert::AreEqual( drawsBefore - 1, static_cast<int>( s.LiveWallRemaining() ) );
			Assert::AreEqual( 1, static_cast<int>( s.players[0].hand.TotalTiles() ), L"the replacement tile" );

			// A pulled North stays visible to everyone.
			Assert::AreEqual( 1, static_cast<int>( s.VisibleTo( 1 )[north] ) );
		}

		TEST_METHOD( FourPlayerHasNoKita )
		{
			Engine engine( MahjongSoul4P(), 55 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			s.players[0].hand.Add( Id( RiichiMahjongTile::NORTH ) );

			Assert::IsTrue( engine.Step( Action::Kita( 0 ) ) == StepResult::ILLEGAL );
		}

		TEST_METHOD( KitaNeedsANorthInHand )
		{
			Engine engine( MahjongSoul3P(), 56 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			s.players[0].hand.Clear();
			s.players[0].hand.Add( Id( RiichiMahjongTile::EAST ) );
			s.players[0].awaitingDiscard = true;

			Assert::IsTrue( engine.Step( Action::Kita( 0 ) ) == StepResult::ILLEGAL );
		}
	};

	TEST_CLASS( Outcomes )
	{
	public:
		TEST_METHOD( KyuushuNeedsNineTerminalKinds )
		{
			Engine engine( MahjongSoul4P(), 66 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			// Eight distinct terminals and honours is not enough.
			s.players[0].hand.Clear();
			const TileId eight[] = {
				Id( RiichiMahjongTile::MAN_1 ), Id( RiichiMahjongTile::MAN_9 ),
				Id( RiichiMahjongTile::PIN_1 ), Id( RiichiMahjongTile::PIN_9 ),
				Id( RiichiMahjongTile::SOU_1 ), Id( RiichiMahjongTile::SOU_9 ),
				Id( RiichiMahjongTile::EAST ),  Id( RiichiMahjongTile::SOUTH ),
			};
			for ( const TileId t : eight ) s.players[0].hand.Add( t );
			s.players[0].awaitingDiscard = true;

			Assert::IsTrue( engine.Step( Action::Kyuushu( 0 ) ) == StepResult::ILLEGAL );

			s.players[0].hand.Add( Id( RiichiMahjongTile::WEST ) );
			Assert::IsTrue( engine.Step( Action::Kyuushu( 0 ) ) == StepResult::HAND_ENDED );
			Assert::IsTrue( s.result.outcome == HandOutcome::ABORT_KYUUSHU );
		}

		TEST_METHOD( KyuushuOnlyOnTheFirstGoAround )
		{
			Engine engine( MahjongSoul4P(), 67 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			s.firstGoAround = false;

			s.players[0].hand.Clear();
			const TileId nine[] = {
				Id( RiichiMahjongTile::MAN_1 ), Id( RiichiMahjongTile::MAN_9 ),
				Id( RiichiMahjongTile::PIN_1 ), Id( RiichiMahjongTile::PIN_9 ),
				Id( RiichiMahjongTile::SOU_1 ), Id( RiichiMahjongTile::SOU_9 ),
				Id( RiichiMahjongTile::EAST ),  Id( RiichiMahjongTile::SOUTH ),
				Id( RiichiMahjongTile::WEST ),
			};
			for ( const TileId t : nine ) s.players[0].hand.Add( t );
			s.players[0].awaitingDiscard = true;

			Assert::IsTrue( engine.Step( Action::Kyuushu( 0 ) ) == StepResult::ILLEGAL );
		}

		TEST_METHOD( HaiteiIsWinningOnTheLastLiveTile )
		{
			Engine engine( MahjongSoul4P(), 88 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			SetHand( s.players[0], WINNING_14 );
			s.players[0].drawn = MakeInstance( Id( RiichiMahjongTile::PIN_1 ) );
			s.liveWallHead = s.liveWallTail; // the tile in hand was the last one

			Assert::IsTrue( engine.Step( Action::Tsumo( 0 ) ) == StepResult::HAND_ENDED );

			Assert::IsTrue( s.result.outcome == HandOutcome::TSUMO );
			Assert::AreEqual( 0, static_cast<int>( s.result.winner ) );
			Assert::IsTrue( s.result.haitei );
			Assert::IsFalse( s.result.rinshan );
			Assert::IsFalse( s.result.houtei );
		}

		TEST_METHOD( RinshanIsWinningOnAReplacementTile )
		{
			Engine engine( MahjongSoul4P(), 89 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId t = Id( RiichiMahjongTile::PIN_7 );
			s.players[0].hand.Clear();
			for ( int i = 0; i < 4; ++i ) s.players[0].hand.Add( t );
			s.players[0].awaitingDiscard = true;

			engine.Step( Action::Ankan( 0, t ) );

			// With the kan melded, eleven concealed tiles complete the hand.
			SetHand( s.players[0], "123456789m11p" );
			Assert::IsTrue( engine.Step( Action::Tsumo( 0 ) ) == StepResult::HAND_ENDED );

			Assert::IsTrue( s.result.rinshan );
			Assert::IsFalse( s.result.haitei, L"a dead wall tile is never haitei" );
		}

		TEST_METHOD( HouteiIsWinningOnTheFinalDiscard )
		{
			Engine engine( MahjongSoul4P(), 90 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId winning = Id( RiichiMahjongTile::PIN_1 );
			SetHand( s.players[0], "1p" );
			s.players[0].drawn = MakeInstance( winning );

			SetHand( s.players[2], TENPAI_13 );  // waiting on 1p
			SetHand( s.players[1], "1m" );
			SetHand( s.players[3], "1m" );

			s.liveWallHead = s.liveWallTail;

			engine.Step( Action::Discard( 0, MakeInstance( winning ) ) );
			Assert::IsTrue( engine.Step( Action::Ron( 2 ) ) == StepResult::HAND_ENDED );

			Assert::IsTrue( s.result.outcome == HandOutcome::RON );
			Assert::AreEqual( 2, static_cast<int>( s.result.winner ) );
			Assert::AreEqual( 0, static_cast<int>( s.result.loser ) );
			Assert::IsTrue( s.result.houtei );
		}

		TEST_METHOD( FourWindsAbortsTheHand )
		{
			Engine engine( MahjongSoul4P(), 91 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			const TileId east = Id( RiichiMahjongTile::EAST );

			for ( uint8_t seat = 0; seat < 4; ++seat )
			{
				s.players[seat].hand.Clear();
				s.players[seat].hand.Add( east );
			}

			for ( uint8_t seat = 0; seat < 4; ++seat )
			{
				s.players[seat].awaitingDiscard = true;
				engine.Step( Action::Discard( seat, MakeInstance( east ) ) );

				for ( uint8_t other = 0; other < 4; ++other )
				{
					if ( ( s.pendingCallers & ( 1u << other ) ) != 0 ) engine.Step( Action::Pass( other ) );
				}
			}

			Assert::IsTrue( s.phase == Phase::HAND_OVER );
			Assert::IsTrue( s.result.outcome == HandOutcome::ABORT_FOUR_WINDS );
		}
	};

	// =================================================================
	// M2: legal action generation, priority, furiten and kuikae.
	// =================================================================

	namespace
	{
		Counts34 CountsOf( const char *tenhou )
		{
			Counts34 counts{};
			AkaMask  aka = AKA_NONE;
			Assert::IsTrue( CountsFromTenhouString( tenhou, counts, aka ), L"test hand should parse" );
			return counts;
		}

		uint64_t WaitMaskOf( const char *tenhou, const uint8_t meldCount = 0 )
		{
			return WaitingTiles( CountsOf( tenhou ), meldCount );
		}

		uint64_t MaskOf( const std::initializer_list<RiichiMahjongTile> tiles )
		{
			uint64_t mask = 0;
			for ( const RiichiMahjongTile t : tiles ) mask |= ( 1ULL << Id( t ) );
			return mask;
		}

		// Melds a pon onto a seat without going through the call machinery.
		void GivePon( Player &p, const TileId t )
		{
			p.melds[p.meldCount].type = MeldType::PON;
			p.melds[p.meldCount].base = t;
			++p.meldCount;
		}
	}

	TEST_CLASS( WinShapes )
	{
	public:
		TEST_METHOD( StandardHandsAreRecognised )
		{
			const char *winning[] = {
				"123456789m11123p",  // three runs, a run and a pair
				"111222333444m55p",  // four triplets
				"123123123m11p123s", // repeated runs across suits
				"11122233344455z",   // honours only
			};

			for ( const char *text : winning )
			{
				Assert::IsTrue( IsWinningHand( CountsOf( text ), 0 ), L"should be a complete hand" );
			}
		}

		TEST_METHOD( IncompleteHandsAreRejected )
		{
			const char *losing[] = {
				"123456789m1123p",   // thirteen tiles, not fourteen
				"123456789m11223p",  // fourteen tiles, no decomposition
				"1245678m11123p99s",
			};

			for ( const char *text : losing )
			{
				Assert::IsFalse( IsWinningHand( CountsOf( text ), 0 ), L"should not be complete" );
			}
		}

		TEST_METHOD( SevenPairsNeedsSevenDistinctPairs )
		{
			Assert::IsTrue( IsSevenPairs( CountsOf( "1133557799m1133p" ) ) );

			// Four of a kind is not two of the seven pairs.
			Assert::IsFalse( IsSevenPairs( CountsOf( "111133557799m11p" ) ) );

			// Only a concealed hand may be seven pairs.
			Assert::IsTrue( IsWinningHand( CountsOf( "1133557799m1133p" ), 0 ) );
		}

		TEST_METHOD( ThirteenOrphansIsRecognised )
		{
			Assert::IsTrue( IsThirteenOrphans( CountsOf( "119m19p19s1234567z" ) ) );
			Assert::IsTrue( IsWinningHand( CountsOf( "119m19p19s1234567z" ), 0 ) );

			// Missing one of the thirteen kinds.
			Assert::IsFalse( IsThirteenOrphans( CountsOf( "1199m19p19s123456z" ) ) );
		}

		TEST_METHOD( MeldedHandsSupplyFewerSets )
		{
			// One meld: eleven concealed tiles making three sets and a pair.
			Assert::IsTrue( IsWinningHand( CountsOf( "123456789m11p" ), 1 ) );
			Assert::IsFalse( IsWinningHand( CountsOf( "123456789m11p" ), 0 ), L"wrong tile count for a closed hand" );

			// Three melds: five concealed tiles making one set and a pair.
			Assert::IsTrue( IsWinningHand( CountsOf( "123m11p" ), 3 ) );

			// Four melds: just the pair.
			Assert::IsTrue( IsWinningHand( CountsOf( "11p" ), 4 ) );
		}

		TEST_METHOD( WaitsAreEnumerated )
		{
			// 123m 456m 789m + 11p pair + 23p, so 1p or 4p completes it.
			Assert::AreEqual( MaskOf( { RiichiMahjongTile::PIN_1, RiichiMahjongTile::PIN_4 } ),
				WaitMaskOf( "123456789m1123p" ) );

			// Four complete sets and a lone tile is a tanki on that tile.
			Assert::AreEqual( MaskOf( { RiichiMahjongTile::SOU_5 } ),
				WaitMaskOf( "123456789m123p5s" ) );
		}

		TEST_METHOD( ClosedAndEdgeWaits )
		{
			// 13m is a kanchan, closed on 2m.
			Assert::AreEqual( MaskOf( { RiichiMahjongTile::MAN_2 } ),
				WaitMaskOf( "13m456789m11123p" ) );

			// 12m is a penchan, only 3m serves.
			Assert::AreEqual( MaskOf( { RiichiMahjongTile::MAN_3 } ),
				WaitMaskOf( "12m456789m11123p" ) );
		}

		TEST_METHOD( TenpaiAgreesWithTheWaitSet )
		{
			Assert::IsTrue( IsTenpai( CountsOf( "123456789m1123p" ), 0 ) );
			Assert::IsFalse( IsTenpai( CountsOf( "123456789m1129p" ), 0 ) );
			Assert::AreEqual( uint64_t( 0 ), WaitMaskOf( "123456789m1129p" ) );
		}

		TEST_METHOD( ThirteenOrphansWaitsOnAllThirteen )
		{
			const uint64_t waits = WaitMaskOf( "19m19p19s1234567z" );

			uint8_t count = 0;
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				if ( ( waits & ( 1ULL << t ) ) != 0 )
				{
					Assert::IsTrue( IsTerminalOrHonor( t ) );
					++count;
				}
			}

			Assert::AreEqual( 13, static_cast<int>( count ) );
		}
	};

	TEST_CLASS( Furiten )
	{
	public:
		TEST_METHOD( AWaitInYourOwnPondBlocksRon )
		{
			Engine engine( MahjongSoul4P(), 100 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			SetHand( s.players[2], TENPAI_13 ); // waits on 1p and 4p
			Assert::IsFalse( IsFuriten( s, 2 ) );

			// Having discarded a 4p earlier, seat 2 can no longer ron either.
			s.players[2].discards[0]  = MakeInstance( Id( RiichiMahjongTile::PIN_4 ) );
			s.players[2].discardCount = 1;

			Assert::IsTrue( IsFuriten( s, 2 ), L"any waited-on tile in your own pond is enough" );

			const TileId winning = Id( RiichiMahjongTile::PIN_1 );
			SetHand( s.players[0], "1p" );
			s.players[0].drawn = MakeInstance( winning );
			SetHand( s.players[1], "1m" );
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( winning ) ) );

			Assert::IsTrue( engine.Step( Action::Ron( 2 ) ) == StepResult::ILLEGAL );
		}

		TEST_METHOD( DecliningAWinningTileCausesTemporaryFuriten )
		{
			Engine engine( MahjongSoul4P(), 101 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId winning = Id( RiichiMahjongTile::PIN_1 );
			SetHand( s.players[0], "1p" );
			s.players[0].drawn = MakeInstance( winning );
			SetHand( s.players[1], "1m" );
			SetHand( s.players[2], TENPAI_13 );
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( winning ) ) );
			Assert::IsTrue( ( s.pendingCallers & 0b0100 ) != 0, L"seat 2 was offered the ron" );

			engine.Step( Action::Pass( 2 ) );

			Assert::IsTrue( s.players[2].furitenTemporary );
			Assert::IsFalse( s.players[2].furitenPermanent, L"not in riichi, so it lifts" );
			Assert::IsTrue( IsFuriten( s, 2 ) );
		}

		TEST_METHOD( TemporaryFuritenLiftsOnTheNextDraw )
		{
			Engine engine( MahjongSoul4P(), 102 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			s.players[2].furitenTemporary = true;

			// Walk the turn round to seat 2 so that it draws.
			int guard = 0;
			while ( s.currentPlayer != 2 && s.phase != Phase::HAND_OVER && guard++ < 20 )
			{
				engine.Step( Action::Discard( s.currentPlayer, s.players[s.currentPlayer].drawn ) );
				PassEveryone( engine );
			}

			Assert::AreEqual( 2, static_cast<int>( s.currentPlayer ) );
			Assert::IsFalse( s.players[2].furitenTemporary, L"drawing clears it" );
		}

		TEST_METHOD( RiichiFuritenNeverLifts )
		{
			Engine engine( MahjongSoul4P(), 103 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId winning = Id( RiichiMahjongTile::PIN_1 );
			SetHand( s.players[0], "1p" );
			s.players[0].drawn = MakeInstance( winning );
			SetHand( s.players[1], "1m" );
			SetHand( s.players[2], TENPAI_13 );
			SetHand( s.players[3], "1m" );

			// Seat 2 has already declared, so its wait can never change again.
			s.players[2].riichiDeclared = true;

			engine.Step( Action::Discard( 0, MakeInstance( winning ) ) );
			engine.Step( Action::Pass( 2 ) );

			Assert::IsTrue( s.players[2].furitenPermanent );

			// Even a fresh draw leaves it furiten.
			s.players[2].furitenTemporary = false;
			Assert::IsTrue( IsFuriten( s, 2 ) );
		}

		TEST_METHOD( FuritenStillAllowsTsumo )
		{
			Engine engine( MahjongSoul4P(), 104 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			SetHand( s.players[0], WINNING_14 );
			s.players[0].drawn            = MakeInstance( Id( RiichiMahjongTile::PIN_1 ) );
			s.players[0].furitenPermanent = true;

			ActionList legal;
			LegalActions( s, 0, legal );

			Assert::IsTrue( legal.ContainsType( ActionType::TSUMO ), L"furiten only blocks ron" );
		}
	};

	TEST_CLASS( CallPriority )
	{
	public:
		TEST_METHOD( RonOutranksPon )
		{
			Engine engine( MahjongSoul4P(), 200 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId winning = Id( RiichiMahjongTile::PIN_1 );
			SetHand( s.players[0], "1p" );
			s.players[0].drawn = MakeInstance( winning );
			SetHand( s.players[1], "11p9s" );   // could pon
			SetHand( s.players[2], TENPAI_13 ); // could ron
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( winning ) ) );

			Assert::IsTrue( engine.Step( Action::Pon( 1, winning ) ) == StepResult::OK );
			Assert::IsTrue( engine.Step( Action::Ron( 2 ) ) == StepResult::HAND_ENDED );

			Assert::IsTrue( s.result.outcome == HandOutcome::RON );
			Assert::AreEqual( 2, static_cast<int>( s.result.winner ) );
			Assert::AreEqual( 0, static_cast<int>( s.players[1].meldCount ), L"the pon never happened" );
		}

		TEST_METHOD( PonOutranksChi )
		{
			Engine engine( MahjongSoul4P(), 201 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId called = Id( RiichiMahjongTile::SOU_3 );
			SetHand( s.players[0], "3s" );
			s.players[0].drawn = MakeInstance( called );

			SetHand( s.players[1], "129s" );  // the left neighbour, could chi
			SetHand( s.players[2], "339s" );  // could pon
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( called ) ) );

			engine.Step( Action::Chi( 1, Id( RiichiMahjongTile::SOU_1 ) ) );
			engine.Step( Action::Pon( 2, called ) );

			Assert::AreEqual( 2, static_cast<int>( s.currentPlayer ), L"the pon takes the turn" );
			Assert::AreEqual( 1, static_cast<int>( s.players[2].meldCount ) );
			Assert::IsTrue( s.players[2].melds[0].type == MeldType::PON );
			Assert::AreEqual( 0, static_cast<int>( s.players[1].meldCount ), L"the chi never happened" );
		}

		TEST_METHOD( DoubleRonPaysBothClaimants )
		{
			Engine engine( MahjongSoul4P(), 202 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId winning = Id( RiichiMahjongTile::PIN_1 );
			SetHand( s.players[0], "1p" );
			s.players[0].drawn = MakeInstance( winning );
			SetHand( s.players[1], TENPAI_13 );
			SetHand( s.players[2], TENPAI_13 );
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( winning ) ) );
			engine.Step( Action::Ron( 1 ) );
			Assert::IsTrue( engine.Step( Action::Ron( 2 ) ) == StepResult::HAND_ENDED );

			Assert::IsTrue( s.result.outcome == HandOutcome::RON );
			Assert::AreEqual( 0b0110, static_cast<int>( s.result.winnerMask ), L"both seats collect" );
			Assert::AreEqual( 1, static_cast<int>( s.result.winner ), L"nearest the discarder heads the result" );
			Assert::AreEqual( 0, static_cast<int>( s.result.loser ) );
		}

		TEST_METHOD( HeadBumpWhenDoubleRonIsDisabled )
		{
			Rules rules = MahjongSoul4P();
			rules.doubleRon = false;

			Engine engine( rules, 203 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId winning = Id( RiichiMahjongTile::PIN_1 );
			SetHand( s.players[0], "1p" );
			s.players[0].drawn = MakeInstance( winning );
			SetHand( s.players[1], TENPAI_13 );
			SetHand( s.players[2], TENPAI_13 );
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( winning ) ) );
			engine.Step( Action::Ron( 1 ) );
			engine.Step( Action::Ron( 2 ) );

			Assert::AreEqual( 0b0010, static_cast<int>( s.result.winnerMask ), L"only the nearest claimant" );
			Assert::AreEqual( 1, static_cast<int>( s.result.winner ) );
		}

		TEST_METHOD( TripleRonAbortsTheHand )
		{
			Engine engine( MahjongSoul4P(), 204 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId winning = Id( RiichiMahjongTile::PIN_1 );
			SetHand( s.players[0], "1p" );
			s.players[0].drawn = MakeInstance( winning );
			for ( uint8_t seat = 1; seat < 4; ++seat ) SetHand( s.players[seat], TENPAI_13 );

			engine.Step( Action::Discard( 0, MakeInstance( winning ) ) );
			engine.Step( Action::Ron( 1 ) );
			engine.Step( Action::Ron( 2 ) );
			Assert::IsTrue( engine.Step( Action::Ron( 3 ) ) == StepResult::HAND_ENDED );

			Assert::IsTrue( s.result.outcome == HandOutcome::ABORT_TRIPLE_RON );
			Assert::AreEqual( static_cast<int>( INVALID_SEAT ), static_cast<int>( s.result.winner ) );
		}
	};

	TEST_CLASS( Kuikae )
	{
	public:
		TEST_METHOD( TheClaimedTileCannotGoStraightBackOut )
		{
			Engine engine( MahjongSoul4P(), 300 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId called = Id( RiichiMahjongTile::SOU_3 );
			SetHand( s.players[0], "3s" );
			s.players[0].drawn = MakeInstance( called );
			SetHand( s.players[1], "3339s" ); // pons and still holds a third 3s
			SetHand( s.players[2], "1m" );
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( called ) ) );
			engine.Step( Action::Pon( 1, called ) );

			Assert::AreEqual( 1, static_cast<int>( s.currentPlayer ) );

			ActionList legal;
			LegalActions( s, 1, legal );

			Assert::IsFalse( legal.Contains( Action::Discard( 1, MakeInstance( called ) ) ),
				L"swapping the claimed tile straight back is kuikae" );
			Assert::IsTrue( legal.Contains( Action::Discard( 1, MakeInstance( Id( RiichiMahjongTile::SOU_9 ) ) ) ) );

			Assert::IsTrue( engine.Step( Action::Discard( 1, MakeInstance( called ) ) ) == StepResult::ILLEGAL );
		}

		TEST_METHOD( AChiAtTheEndOfARunForbidsTheFarEnd )
		{
			Engine engine( MahjongSoul4P(), 301 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			// Seat 0 throws 1s, seat 1 chis it as 1s2s3s while holding a 4s.
			const TileId called = Id( RiichiMahjongTile::SOU_1 );
			SetHand( s.players[0], "1s" );
			s.players[0].drawn = MakeInstance( called );
			SetHand( s.players[1], "2349s" );
			SetHand( s.players[2], "1m" );
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( called ) ) );
			engine.Step( Action::Chi( 1, called ) );

			ActionList legal;
			LegalActions( s, 1, legal );

			// 4s would leave the same wait with the claimed tile swapped in.
			Assert::IsFalse( legal.Contains( Action::Discard( 1, MakeInstance( Id( RiichiMahjongTile::SOU_4 ) ) ) ),
				L"the far end of the run is barred too" );
			Assert::IsTrue( legal.Contains( Action::Discard( 1, MakeInstance( Id( RiichiMahjongTile::SOU_9 ) ) ) ) );
		}

		TEST_METHOD( TheRestrictionLastsExactlyOneDiscard )
		{
			Engine engine( MahjongSoul4P(), 302 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId called = Id( RiichiMahjongTile::SOU_3 );
			SetHand( s.players[0], "3s" );
			s.players[0].drawn = MakeInstance( called );
			SetHand( s.players[1], "3339s" );
			SetHand( s.players[2], "1m" );
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( called ) ) );
			engine.Step( Action::Pon( 1, called ) );

			Assert::AreNotEqual( uint64_t( 0 ), s.forbiddenDiscards );

			engine.Step( Action::Discard( 1, MakeInstance( Id( RiichiMahjongTile::SOU_9 ) ) ) );
			Assert::AreEqual( uint64_t( 0 ), s.forbiddenDiscards, L"cleared by the discard it constrained" );
		}

		TEST_METHOD( TheMaskCoversOnlyWhatItShould )
		{
			const TileId sou1 = Id( RiichiMahjongTile::SOU_1 );
			const TileId sou3 = Id( RiichiMahjongTile::SOU_3 );
			const TileId sou4 = Id( RiichiMahjongTile::SOU_4 );

			// Claiming the bottom of 1s2s3s also bars 4s.
			Assert::AreEqual( MaskOf( { RiichiMahjongTile::SOU_1, RiichiMahjongTile::SOU_4 } ),
				KuikaeMask( MeldType::CHI, sou1, sou1 ) );

			// Claiming the top of 1s2s3s bars nothing below, there being no 0s.
			Assert::AreEqual( MaskOf( { RiichiMahjongTile::SOU_3 } ),
				KuikaeMask( MeldType::CHI, sou1, sou3 ) );

			// Claiming the middle bars only the tile itself.
			Assert::AreEqual( MaskOf( { RiichiMahjongTile::SOU_4 } ),
				KuikaeMask( MeldType::CHI, sou3, sou4 ) );

			// A pon only ever bars its own tile.
			Assert::AreEqual( MaskOf( { RiichiMahjongTile::SOU_3 } ),
				KuikaeMask( MeldType::PON, sou3, sou3 ) );
		}
	};

	TEST_CLASS( Chankan )
	{
	public:
		TEST_METHOD( AnAddedKanCanBeRobbed )
		{
			Engine engine( MahjongSoul4P(), 400 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId kanTile = Id( RiichiMahjongTile::MAN_3 );

			SetHand( s.players[0], "3m" );
			GivePon( s.players[0], kanTile );
			s.players[0].awaitingDiscard = true;

			// Seat 2 is waiting on exactly that tile.
			SetHand( s.players[2], "12456789m11123p" );
			SetHand( s.players[1], "1m" );
			SetHand( s.players[3], "1m" );

			Assert::IsTrue( engine.Step( Action::Shouminkan( 0, kanTile ) ) == StepResult::OK );
			Assert::IsTrue( s.awaitingChankan, L"the added tile is open to being robbed" );
			Assert::IsTrue( s.phase == Phase::CALL );

			Assert::IsTrue( engine.Step( Action::Ron( 2 ) ) == StepResult::HAND_ENDED );

			Assert::IsTrue( s.result.outcome == HandOutcome::RON );
			Assert::IsTrue( s.result.chankan );
			Assert::AreEqual( 2, static_cast<int>( s.result.winner ) );
			Assert::AreEqual( 0, static_cast<int>( s.result.loser ) );
		}

		TEST_METHOD( OnlyRonAndPassAnswerARobbableKan )
		{
			Engine engine( MahjongSoul4P(), 401 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId kanTile = Id( RiichiMahjongTile::MAN_3 );
			SetHand( s.players[0], "3m" );
			GivePon( s.players[0], kanTile );
			s.players[0].awaitingDiscard = true;

			// Seat 2 can rob it, which is what holds the window open.
			SetHand( s.players[2], "12456789m11123p" );
			SetHand( s.players[1], "3399m" ); // could otherwise pon a 3m
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Shouminkan( 0, kanTile ) );
			Assert::IsTrue( s.awaitingChankan );

			// The robber is offered a ron and nothing but a ron.
			ActionList legal;
			LegalActions( s, 2, legal );
			Assert::IsTrue( legal.ContainsType( ActionType::RON ) );
			Assert::IsTrue( legal.ContainsType( ActionType::PASS ) );
			Assert::AreEqual( 2, static_cast<int>( legal.count ), L"only pass and ron answer a kan" );

			// A tile that is already melded cannot be ponned out of the meld.
			Assert::IsTrue( engine.Step( Action::Pon( 1, kanTile ) ) == StepResult::ILLEGAL );
		}

		TEST_METHOD( AnUnrobbedKanTakesItsReplacement )
		{
			Engine engine( MahjongSoul4P(), 402 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId kanTile = Id( RiichiMahjongTile::MAN_3 );
			SetHand( s.players[0], "3m" );
			GivePon( s.players[0], kanTile );
			s.players[0].awaitingDiscard = true;

			for ( uint8_t seat = 1; seat < 4; ++seat ) SetHand( s.players[seat], "1m" );

			const int drawsBefore = s.LiveWallRemaining();

			// Nobody can rob it, so the window closes inside the same step.
			Assert::IsTrue( engine.Step( Action::Shouminkan( 0, kanTile ) ) == StepResult::OK );

			Assert::IsFalse( s.awaitingChankan );
			Assert::IsTrue( s.phase == Phase::DISCARD );
			Assert::AreEqual( 0, static_cast<int>( s.currentPlayer ) );
			Assert::AreEqual( 1, static_cast<int>( s.deadWallDraws ) );
			Assert::AreEqual( drawsBefore - 1, static_cast<int>( s.LiveWallRemaining() ) );
			Assert::IsTrue( s.pendingDoraFlip, L"an added kan flips only after the discard" );
			Assert::IsTrue( s.players[0].melds[0].type == MeldType::SHOUMINKAN );
		}
	};

	TEST_CLASS( RiichiLegality )
	{
	public:
		TEST_METHOD( RiichiNeedsTenpai )
		{
			Engine engine( MahjongSoul4P(), 500 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			// Nowhere near tenpai.
			SetHand( s.players[0], "159m159p159s1234z" );
			s.players[0].drawn = MakeInstance( Id( RiichiMahjongTile::EAST ) );

			ActionList legal;
			LegalActions( s, 0, legal );
			Assert::IsFalse( legal.ContainsType( ActionType::RIICHI ) );
		}

		TEST_METHOD( RiichiOnlyOffersDiscardsThatLeaveTenpai )
		{
			Engine engine( MahjongSoul4P(), 501 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			SetHand( s.players[0], "123456789m1123p9s" );
			s.players[0].drawn = MakeInstance( Id( RiichiMahjongTile::SOU_9 ) );

			ActionList legal;
			LegalActions( s, 0, legal );

			// Two discards keep the hand tenpai: shedding the 9s waits on 1p and
			// 4p, and shedding a 1p leaves a tanki on the 9s. Nothing else does.
			uint64_t offered = 0;
			for ( const Action &a : legal )
			{
				if ( a.type == ActionType::RIICHI ) offered |= ( 1ULL << InstanceTile( a.tile ) );
			}

			Assert::AreEqual( MaskOf( { RiichiMahjongTile::SOU_9, RiichiMahjongTile::PIN_1 } ), offered );
		}

		TEST_METHOD( RiichiNeedsFourTilesLeftInTheWall )
		{
			Engine engine( MahjongSoul4P(), 502 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			SetHand( s.players[0], "123456789m1123p9s" );
			s.players[0].drawn = MakeInstance( Id( RiichiMahjongTile::SOU_9 ) );
			s.liveWallHead = static_cast<uint8_t>( s.liveWallTail - 3 );

			ActionList legal;
			LegalActions( s, 0, legal );
			Assert::IsFalse( legal.ContainsType( ActionType::RIICHI ), L"three tiles is not enough" );

			s.liveWallHead = static_cast<uint8_t>( s.liveWallTail - 4 );
			LegalActions( s, 0, legal );
			Assert::IsTrue( legal.ContainsType( ActionType::RIICHI ) );
		}

		TEST_METHOD( AnkanDuringRiichiMustNotDisturbTheWait )
		{
			Engine engine( MahjongSoul4P(), 503 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			// 111m 456m 789m 11p 23p waits on 1p and 4p; a fourth 1m changes
			// nothing about that.
			SetHand( s.players[0], "1111456789m1123p" );
			s.players[0].drawn          = MakeInstance( Id( RiichiMahjongTile::MAN_1 ) );
			s.players[0].riichiDeclared = true;

			ActionList legal;
			LegalActions( s, 0, legal );
			Assert::IsTrue( legal.Contains( Action::Ankan( 0, Id( RiichiMahjongTile::MAN_1 ) ) ),
				L"the wait is untouched, so the kan is allowed" );

			// The same four tiles, but they are not what was just drawn.
			s.players[0].drawn = MakeInstance( Id( RiichiMahjongTile::PIN_2 ) );
			LegalActions( s, 0, legal );
			Assert::IsFalse( legal.ContainsType( ActionType::ANKAN ),
				L"a declared hand may only kan the tile it just drew" );
		}

		TEST_METHOD( ADeclaredHandCannotCall )
		{
			Engine engine( MahjongSoul4P(), 504 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			const TileId called = Id( RiichiMahjongTile::SOU_3 );
			SetHand( s.players[0], "3s" );
			s.players[0].drawn = MakeInstance( called );

			SetHand( s.players[1], "3339s" );
			s.players[1].riichiDeclared = true;
			SetHand( s.players[2], "1m" );
			SetHand( s.players[3], "1m" );

			engine.Step( Action::Discard( 0, MakeInstance( called ) ) );

			ActionList legal;
			LegalActions( s, 1, legal );
			Assert::IsFalse( legal.ContainsType( ActionType::PON ), L"riichi locks the hand" );
			Assert::IsFalse( legal.ContainsType( ActionType::CHI ) );
		}
	};

	// -----------------------------------------------------------------
	// The invariant that holds generation and application together. If
	// these two ever disagree, a search will either explore states the
	// engine refuses to enter or miss states it would accept.
	// -----------------------------------------------------------------
	TEST_CLASS( GenerationMatchesApplication )
	{
	public:
		TEST_METHOD( EveryGeneratedActionIsAccepted )
		{
			Engine engine( MahjongSoul4P(), 4242 );
			engine.StartHand( 0 );

			int guard = 0;
			int checked = 0;

			while ( engine.State().phase != Phase::HAND_OVER && guard++ < 2000 )
			{
				const GameState &s = engine.State();

				const uint8_t actingSeat = ( s.phase == Phase::DISCARD )
					? s.currentPlayer
					: static_cast<uint8_t>( [&]
						{
							for ( uint8_t seat = 0; seat < s.PlayerCount(); ++seat )
							{
								if ( ( s.pendingCallers & ( 1u << seat ) ) != 0 ) return seat;
							}
							return INVALID_SEAT;
						}( ) );

				Assert::IsTrue( actingSeat < s.PlayerCount(), L"somebody must be able to act" );

				ActionList legal;
				LegalActions( s, actingSeat, legal );
				Assert::IsTrue( legal.count > 0, L"a live state must offer at least one action" );

				// Each option must be accepted, tried against its own copy so
				// the real hand is not disturbed.
				for ( const Action &a : legal )
				{
					Engine probe = engine;
					const StepResult r = probe.Step( a );
					Assert::IsTrue( r != StepResult::ILLEGAL, L"a generated action must be accepted" );
					++checked;
				}

				engine.Step( legal[0] );
			}

			Assert::IsTrue( checked > 100, L"the walk should have covered real ground" );
		}

		TEST_METHOD( UngeneratedActionsAreRejected )
		{
			Engine engine( MahjongSoul4P(), 4243 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();

			ActionList legal;
			LegalActions( s, 0, legal );

			// Probe the whole action space and confirm Step agrees exactly with
			// what was generated.
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				const Action candidates[] = {
					Action::Discard( 0, MakeInstance( t ) ),
					Action::Riichi( 0, MakeInstance( t ) ),
					Action::Ankan( 0, t ),
					Action::Shouminkan( 0, t ),
					Action::Pon( 0, t ),
					Action::Chi( 0, t ),
				};

				for ( const Action &a : candidates )
				{
					Engine probe = engine;
					const bool generated = legal.Contains( a );
					const bool accepted  = probe.Step( a ) != StepResult::ILLEGAL;

					Assert::AreEqual( generated, accepted, L"Step and LegalActions must agree" );
				}
			}
		}

		TEST_METHOD( SeatsWithNoTurnGetNoActions )
		{
			Engine engine( MahjongSoul4P(), 4244 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();
			ActionList legal;

			for ( uint8_t seat = 1; seat < 4; ++seat )
			{
				Assert::AreEqual( 0, static_cast<int>( LegalActions( s, seat, legal ) ),
					L"it is not their turn" );
			}

			Assert::IsTrue( LegalActions( s, 0, legal ) > 0 );
		}
	};

	// =================================================================
	// M3: shanten and ukeire.
	// =================================================================

	namespace
	{
		int8_t ShantenOf( const char *tenhou, const uint8_t meldCount = 0 )
		{
			return Shanten( CountsOf( tenhou ), meldCount );
		}

		// Deals a random legal hand of the given size from a fresh 136 tile
		// pool, so no kind ever appears five times.
		Counts34 RandomHand( Rng &rng, const int tiles, const bool sanmaOnly = false )
		{
			Counts34 pool{};
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				pool[t] = ( sanmaOnly && !IsSanmaTile( t ) ) ? static_cast<uint8_t>( 0 ) : static_cast<uint8_t>( 4 );
			}

			Counts34 hand{};
			for ( int drawn = 0; drawn < tiles; ++drawn )
			{
				int remaining = 0;
				for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) remaining += pool[t];
				if ( remaining == 0 ) break;

				int pick = static_cast<int>( rng.Below( static_cast<uint32_t>( remaining ) ) );
				for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
				{
					if ( pick < pool[t] )
					{
						--pool[t];
						++hand[t];
						break;
					}
					pick -= pool[t];
				}
			}

			return hand;
		}

		std::wstring ToWString( const std::string &text )
		{
			return std::wstring( text.begin(), text.end() );
		}

		int TotalOf( const Counts34 &counts )
		{
			int total = 0;
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) total += counts[t];
			return total;
		}
	}

	TEST_CLASS( ShantenBasics )
	{
	public:
		TEST_METHOD( CompleteHandsAreMinusOne )
		{
			Assert::AreEqual( -1, static_cast<int>( ShantenOf( WINNING_14 ) ) );
			Assert::AreEqual( -1, static_cast<int>( ShantenOf( "111222333444m55p" ) ) );
			Assert::AreEqual( -1, static_cast<int>( ShantenOf( "1133557799m1133p" ) ), L"seven pairs" );
			Assert::AreEqual( -1, static_cast<int>( ShantenOf( "119m19p19s1234567z" ) ), L"thirteen orphans" );
		}

		TEST_METHOD( TenpaiHandsAreZero )
		{
			Assert::AreEqual( 0, static_cast<int>( ShantenOf( TENPAI_13 ) ) );
			Assert::AreEqual( 0, static_cast<int>( ShantenOf( "123456789m123p5s" ) ), L"tanki" );
			Assert::AreEqual( 0, static_cast<int>( ShantenOf( "13m456789m11123p" ) ), L"kanchan" );
			Assert::AreEqual( 0, static_cast<int>( ShantenOf( "119m19p19s123456z" ) ), L"orphans, one short" );
			Assert::AreEqual( 0, static_cast<int>( ShantenOf( "1133557799m113p" ) ), L"six pairs" );
		}

		TEST_METHOD( AStructurelessHandIsCappedBySevenPairs )
		{
			// Thirteen isolated tiles: no block can form without trading
			// something in, so the standard reading is as bad as it gets.
			const Counts34 scattered = CountsOf( "147m258p369s1234z" );
			Assert::AreEqual( 8, static_cast<int>( StandardShanten( scattered, 0 ) ) );

			// Seven pairs is the better reading of a hand made only of singles:
			// keep seven of them and pair each one up.
			Assert::AreEqual( 6, static_cast<int>( Shanten( scattered, 0 ) ) );
		}

		TEST_METHOD( MeldsCountTowardsTheFourSets )
		{
			// Two melds already down, so the concealed eight tiles need two more
			// sets and the pair.
			Assert::AreEqual( -1, static_cast<int>( ShantenOf( "123456m11p", 2 ) ) );
			Assert::AreEqual( 0,  static_cast<int>( ShantenOf( "12456m11p", 2 ) ) );
			Assert::AreEqual( -1, static_cast<int>( ShantenOf( "11p", 4 ) ) );
		}

		TEST_METHOD( SpecialFormsBeatTheStandardReadingWhenTheyApply )
		{
			// Six pairs and a float: tenpai as seven pairs, but a long way off
			// under the four-sets-and-a-pair reading.
			const Counts34 pairs = CountsOf( "1133557799m113p" );

			Assert::AreEqual( 0, static_cast<int>( SevenPairsShanten( pairs ) ) );
			Assert::AreEqual( 0, static_cast<int>( Shanten( pairs, 0 ) ), L"the better form wins" );
			Assert::IsTrue( StandardShanten( pairs, 0 ) > 0, L"as four sets and a pair it is not close" );

			// Thirteen orphans likewise.
			const Counts34 orphans = CountsOf( "119m19p19s123456z" );
			Assert::AreEqual( 0, static_cast<int>( Shanten( orphans, 0 ) ) );
			Assert::IsTrue( StandardShanten( orphans, 0 ) > 0 );
		}

		TEST_METHOD( SevenPairsCountsDistinctKinds )
		{
			// Four of a kind supplies one pair towards seven, not two, so the
			// duplicate has to be traded out.
			// Five pairs but only six distinct kinds, so a seventh kind has to be
			// found as well: one exchange for the pair, one for the kind.
			Assert::AreEqual( 2, static_cast<int>( SevenPairsShanten( CountsOf( "1111335577m113p" ) ) ) );
			Assert::AreEqual( 0, static_cast<int>( SevenPairsShanten( CountsOf( "1133557799m113p" ) ) ) );
		}

		TEST_METHOD( ThirteenOrphansCountsKindsAndThePair )
		{
			// All thirteen kinds, no pair: the thirteen-sided wait.
			Assert::AreEqual( 0,  static_cast<int>( ThirteenOrphansShanten( CountsOf( "19m19p19s1234567z" ) ) ) );
			Assert::AreEqual( -1, static_cast<int>( ThirteenOrphansShanten( CountsOf( "119m19p19s1234567z" ) ) ) );

			// Twelve kinds with a pair is tenpai on the thirteenth.
			Assert::AreEqual( 0, static_cast<int>( ThirteenOrphansShanten( CountsOf( "119m19p19s123456z" ) ) ) );

			// Twelve kinds and no pair is one away.
			Assert::AreEqual( 1, static_cast<int>( ThirteenOrphansShanten( CountsOf( "159m19p19s123456z" ) ) ) );
		}
	};

	// -----------------------------------------------------------------
	// The equivalence test. The fast path splits a hand by suit, profiles
	// each group and recombines; the reference is one flat recursion over
	// all 34 tiles. They agree only if the grouping and the recombination
	// are both right, which is where the bugs actually live.
	// -----------------------------------------------------------------
	TEST_CLASS( ShantenEquivalence )
	{
	public:
		TEST_METHOD( FastAgreesWithReferenceOnRandomHands )
		{
			Rng rng( 20260824 );

			for ( int i = 0; i < 20000; ++i )
			{
				const Counts34 hand = RandomHand( rng, 13 );

				const int8_t fast = Shanten( hand, 0 );
				const int8_t slow = ShantenReference( hand, 0 );

				if ( fast != slow )
				{
					Assert::AreEqual( static_cast<int>( slow ), static_cast<int>( fast ),
						ToWString( CountsToTenhouString( hand, AKA_NONE ) ).c_str() );
				}
			}
		}

		TEST_METHOD( FastAgreesWithReferenceOnMeldedHands )
		{
			Rng rng( 777 );

			for ( uint8_t melds = 1; melds <= 4; ++melds )
			{
				const int tiles = 13 - 3 * melds;

				for ( int i = 0; i < 4000; ++i )
				{
					const Counts34 hand = RandomHand( rng, tiles );

					const int8_t fast = Shanten( hand, melds );
					const int8_t slow = ShantenReference( hand, melds );

					if ( fast != slow )
					{
						Assert::AreEqual( static_cast<int>( slow ), static_cast<int>( fast ),
							ToWString( CountsToTenhouString( hand, AKA_NONE ) ).c_str() );
					}
				}
			}
		}

		TEST_METHOD( FastAgreesWithReferenceOnFourteenTileHands )
		{
			Rng rng( 31337 );

			for ( int i = 0; i < 10000; ++i )
			{
				const Counts34 hand = RandomHand( rng, 14 );
				Assert::AreEqual( static_cast<int>( ShantenReference( hand, 0 ) ),
					static_cast<int>( Shanten( hand, 0 ) ) );
			}
		}

		TEST_METHOD( FastAgreesWithReferenceOnSanmaHands )
		{
			Rng rng( 108 );

			for ( int i = 0; i < 8000; ++i )
			{
				const Counts34 hand = RandomHand( rng, 13, true );
				Assert::AreEqual( static_cast<int>( ShantenReference( hand, 0 ) ),
					static_cast<int>( Shanten( hand, 0 ) ) );
			}
		}

		// Hands near tenpai are where correctness matters most and where a
		// purely random sample lands least often, so they get their own pass.
		TEST_METHOD( FastAgreesWithReferenceNearTenpai )
		{
			Rng rng( 555 );
			int nearCount = 0;

			for ( int i = 0; i < 40000 && nearCount < 4000; ++i )
			{
				const Counts34 hand = RandomHand( rng, 13 );

				const int8_t slow = ShantenReference( hand, 0 );
				if ( slow > 2 ) continue;

				++nearCount;
				Assert::AreEqual( static_cast<int>( slow ), static_cast<int>( Shanten( hand, 0 ) ) );
			}

			Assert::IsTrue( nearCount > 500, L"the sample should contain plenty of near-tenpai hands" );
		}
	};

	// -----------------------------------------------------------------
	// Cross-checks against the independently written M2 code. These
	// validate the block formula itself, which the two shanten
	// implementations share and so cannot check between themselves.
	// -----------------------------------------------------------------
	TEST_CLASS( ShantenAgreesWithWinCheck )
	{
	public:
		TEST_METHOD( MinusOneMeansAComplete14TileHand )
		{
			Rng rng( 4242 );

			for ( int i = 0; i < 20000; ++i )
			{
				const Counts34 hand = RandomHand( rng, 14 );

				const bool byShanten = Shanten( hand, 0 ) == SHANTEN_COMPLETE;
				const bool byWinCheck = IsWinningHand( hand, 0 );

				Assert::AreEqual( byWinCheck, byShanten, L"shanten -1 must mean a complete hand" );
			}
		}

		// The implication only runs one way. Shanten measures shape and ignores
		// the four-copy limit, so a hand whose only wait is on a tile it
		// already holds all four of is shanten 0 with an empty wait. Karaten is
		// a real state, not a disagreement.
		TEST_METHOD( AnyHandWithAWaitIsShantenZero )
		{
			Rng rng( 909 );

			for ( int i = 0; i < 20000; ++i )
			{
				const Counts34 hand = RandomHand( rng, 13 );

				if ( WaitingTiles( hand, 0 ) != 0 )
				{
					Assert::AreEqual( 0, static_cast<int>( Shanten( hand, 0 ) ),
						L"a hand with a winning tile must be tenpai" );
				}
			}
		}

		TEST_METHOD( MeldedHandsAgreeToo )
		{
			Rng rng( 606 );

			for ( uint8_t melds = 1; melds <= 3; ++melds )
			{
				for ( int i = 0; i < 5000; ++i )
				{
					const Counts34 hand = RandomHand( rng, 13 - 3 * melds );

					if ( WaitingTiles( hand, melds ) != 0 )
					{
						Assert::AreEqual( 0, static_cast<int>( Shanten( hand, melds ) ) );
					}
				}
			}
		}

		TEST_METHOD( KaratenIsTenpaiByShapeWithNothingToWinOn )
		{
			// Three melds down and all four 1m in hand: the shape is a tanki on
			// 1m, but no fifth copy exists to complete it.
			const Counts34 hand = CountsOf( "1111m" );

			Assert::AreEqual( 0, static_cast<int>( Shanten( hand, 3 ) ), L"tenpai by shape" );
			Assert::AreEqual( uint64_t( 0 ), WaitingTiles( hand, 3 ), L"but nothing left to draw" );
			Assert::IsFalse( IsTenpai( hand, 3 ) );
		}

		TEST_METHOD( AnyShantenDisagreementIsKaraten )
		{
			Rng rng( 313 );

			for ( uint8_t melds = 0; melds <= 3; ++melds )
			{
				for ( int i = 0; i < 5000; ++i )
				{
					const Counts34 hand = RandomHand( rng, 13 - 3 * melds );

					if ( Shanten( hand, melds ) != 0 ) continue;
					if ( WaitingTiles( hand, melds ) != 0 ) continue;

					// The only way to be tenpai with no winning tile is for
					// every wait to need a fifth copy.
					bool everyWaitExhausted = true;
					for ( TileId t = 0; t < TILE_KIND_COUNT && everyWaitExhausted; ++t )
					{
						if ( hand[t] >= 4 ) continue;

						Counts34 drawn = hand;
						++drawn[t];
						if ( IsWinningHand( drawn, melds ) ) everyWaitExhausted = false;
					}

					Assert::IsTrue( everyWaitExhausted,
						L"tenpai with no wait must be a hand needing a fifth tile" );
				}
			}
		}
	};

	TEST_CLASS( UkeireTests )
	{
	public:
		TEST_METHOD( UkeireMatchesTheWaitSetAtTenpai )
		{
			// At tenpai the tiles that reduce shanten are exactly the winning
			// tiles.
			const Counts34 hand = CountsOf( TENPAI_13 );

			Assert::AreEqual( WaitingTiles( hand, 0 ), Ukeire( hand, 0 ) );
			Assert::AreEqual( MaskOf( { RiichiMahjongTile::PIN_1, RiichiMahjongTile::PIN_4 } ),
				Ukeire( hand, 0 ) );
		}

		TEST_METHOD( EveryUsefulTileActuallyLowersShanten )
		{
			Rng rng( 2468 );

			for ( int i = 0; i < 3000; ++i )
			{
				const Counts34 hand = RandomHand( rng, 13 );

				const int8_t before = Shanten( hand, 0 );
				const uint64_t useful = Ukeire( hand, 0 );

				for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
				{
					if ( hand[t] >= 4 ) continue;

					Counts34 drawn = hand;
					++drawn[t];

					const bool improves = Shanten( drawn, 0 ) < before;
					const bool listed   = ( useful & ( 1ULL << t ) ) != 0;

					Assert::AreEqual( improves, listed, L"ukeire must list exactly the improving tiles" );
				}
			}
		}

		TEST_METHOD( ACompleteHandHasNoUkeire )
		{
			Assert::AreEqual( uint64_t( 0 ), Ukeire( CountsOf( WINNING_14 ), 0 ) );
		}

		TEST_METHOD( CopiesAreWeightedByWhatRemainsUnseen )
		{
			const Counts34 hand = CountsOf( TENPAI_13 ); // waits on 1p and 4p

			// Nothing seen yet beyond the hand itself: two 1p and four 4p left.
			Counts34 remaining{};
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				remaining[t] = static_cast<uint8_t>( 4 - hand[t] );
			}

			const UkeireResult full = UkeireAgainst( hand, 0, remaining );
			Assert::AreEqual( 0, static_cast<int>( full.shanten ) );
			Assert::AreEqual( 6, static_cast<int>( full.copies ), L"two 1p plus four 4p" );

			// Once every 4p is accounted for, only the two 1p are left.
			remaining[Id( RiichiMahjongTile::PIN_4 )] = 0;
			const UkeireResult starved = UkeireAgainst( hand, 0, remaining );

			Assert::AreEqual( 2, static_cast<int>( starved.copies ) );
			Assert::AreEqual( full.tiles, starved.tiles, L"shape is unchanged by what is left" );
		}

		TEST_METHOD( UnseenFromComplementsVisibleTo )
		{
			Engine engine( MahjongSoul4P(), 900 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();

			const Counts34 visible = s.VisibleTo( 0 );
			const Counts34 unseen  = s.UnseenFrom( 0 );

			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				Assert::AreEqual( static_cast<int>( s.rules.CopiesOf( t ) ),
					static_cast<int>( visible[t] + unseen[t] ), L"seen plus unseen is every copy" );
			}

			// The dealer sees fourteen tiles of its own plus one indicator.
			Assert::AreEqual( 15, TotalOf( visible ) );
		}

		TEST_METHOD( SanmaUnseenPoolExcludesTheMissingManzu )
		{
			Engine engine( MahjongSoul3P(), 901 );
			engine.StartHand( 0 );

			const Counts34 unseen = engine.State().UnseenFrom( 0 );

			for ( TileId t = Id( RiichiMahjongTile::MAN_2 ); t <= Id( RiichiMahjongTile::MAN_8 ); ++t )
			{
				Assert::AreEqual( 0, static_cast<int>( unseen[t] ), L"those tiles do not exist" );
			}
		}
	};

	// =================================================================
	// M4: decomposition, yaku, fu and points.
	// =================================================================

	namespace
	{
		// South seat in an east round, non-dealer, so no wind yaku fires by
		// accident and the payment split is the ordinary one.
		WinContext BaseContext( const bool tsumo, const char *winningTile )
		{
			WinContext ctx;
			ctx.rules       = MahjongSoul4P();
			ctx.byTsumo     = tsumo;
			ctx.winningTile = MakeInstance( TileFromTenhouString( winningTile ) );
			ctx.seatWind    = 1; // south
			ctx.roundWind   = 0; // east
			ctx.isDealer    = false;
			return ctx;
		}

		ScoreResult ScoreClosed( const char *hand, const WinContext &ctx )
		{
			return ScoreHand( CountsOf( hand ), nullptr, 0, ctx );
		}

		bool Holds( const ScoreResult &r, const Yaku y ) { return ( r.yaku & YakuBit( y ) ) != 0; }
	}

	TEST_CLASS( Decomposition_ )
	{
	public:
		TEST_METHOD( AHandCanReadMoreThanOneWay )
		{
			// 111222333m is three triplets or three identical runs, and the two
			// readings score differently.
			DecompositionList readings;
			Assert::IsTrue( Decompose( CountsOf( "111222333m44455p" ), nullptr, 0, readings ) >= 2 );

			bool sawTriplets = false;
			bool sawRuns     = false;

			for ( const Decomposition &d : readings )
			{
				int runs = 0;
				int triplets = 0;

				for ( uint8_t i = 0; i < d.setCount; ++i )
				{
					if ( d.sets[i].type == SetType::RUN ) ++runs;
					if ( d.sets[i].type == SetType::TRIPLET ) ++triplets;
				}

				if ( runs >= 3 ) sawRuns = true;
				if ( triplets >= 3 ) sawTriplets = true;
			}

			Assert::IsTrue( sawRuns && sawTriplets, L"both readings must be offered" );
		}

		TEST_METHOD( CalledSetsAppearInEveryReading )
		{
			Meld melds[1];
			melds[0].type = MeldType::PON;
			melds[0].base = Id( RiichiMahjongTile::WHITE_DRAGON );

			DecompositionList readings;
			Assert::IsTrue( Decompose( CountsOf( "123456789m11p" ), melds, 1, readings ) > 0 );

			for ( const Decomposition &d : readings )
			{
				Assert::AreEqual( 5, static_cast<int>( d.setCount ) );

				bool foundCall = false;
				for ( uint8_t i = 0; i < d.setCount; ++i )
				{
					if ( d.sets[i].fromCall )
					{
						foundCall = true;
						Assert::IsFalse( d.sets[i].concealed, L"a pon is open" );
					}
				}

				Assert::IsTrue( foundCall );
			}
		}

		TEST_METHOD( SpecialFormsAreTheirOwnReading )
		{
			DecompositionList pairs;
			Decompose( CountsOf( "1133557799m1133p" ), nullptr, 0, pairs );
			Assert::IsTrue( pairs.count > 0 );
			Assert::IsTrue( pairs.items[0].sevenPairs );

			DecompositionList orphans;
			Decompose( CountsOf( "119m19p19s1234567z" ), nullptr, 0, orphans );
			Assert::IsTrue( orphans.count > 0 );
			Assert::IsTrue( orphans.items[0].thirteenOrphans );
		}

		TEST_METHOD( AnIncompleteHandHasNoReading )
		{
			DecompositionList readings;
			Assert::AreEqual( 0, static_cast<int>( Decompose( CountsOf( "123456789m11223p" ), nullptr, 0, readings ) ) );
		}
	};

	TEST_CLASS( DoraIndicators )
	{
	public:
		TEST_METHOD( TheIndicatorPointsAtTheNextTile )
		{
			Assert::AreEqual( static_cast<int>( Id( RiichiMahjongTile::MAN_2 ) ),
				static_cast<int>( DoraFromIndicator( Id( RiichiMahjongTile::MAN_1 ) ) ) );
			Assert::AreEqual( static_cast<int>( Id( RiichiMahjongTile::PIN_6 ) ),
				static_cast<int>( DoraFromIndicator( Id( RiichiMahjongTile::PIN_5 ) ) ) );
		}

		TEST_METHOD( EachGroupWrapsWithinItself )
		{
			// Nine wraps to one, and does not spill into the next suit.
			Assert::AreEqual( static_cast<int>( Id( RiichiMahjongTile::MAN_1 ) ),
				static_cast<int>( DoraFromIndicator( Id( RiichiMahjongTile::MAN_9 ) ) ) );
			Assert::AreEqual( static_cast<int>( Id( RiichiMahjongTile::SOU_1 ) ),
				static_cast<int>( DoraFromIndicator( Id( RiichiMahjongTile::SOU_9 ) ) ) );

			// North wraps to east, and the dragons wrap among themselves.
			Assert::AreEqual( static_cast<int>( Id( RiichiMahjongTile::EAST ) ),
				static_cast<int>( DoraFromIndicator( Id( RiichiMahjongTile::NORTH ) ) ) );
			Assert::AreEqual( static_cast<int>( Id( RiichiMahjongTile::WHITE_DRAGON ) ),
				static_cast<int>( DoraFromIndicator( Id( RiichiMahjongTile::RED_DRAGON ) ) ) );
		}
	};

	TEST_CLASS( YakuDetection )
	{
	public:
		TEST_METHOD( Pinfu )
		{
			// All runs, a valueless pair, and a two-sided wait completed by the 2m.
			const ScoreResult r = ScoreClosed( "234567m234567p11s", BaseContext( false, "2m" ) );

			Assert::IsTrue( r.valid );
			Assert::IsTrue( Holds( r, Yaku::PINFU ) );
			Assert::AreEqual( 1, static_cast<int>( r.han ) );
			Assert::AreEqual( 30, static_cast<int>( r.fu ), L"a closed pinfu ron is thirty" );
		}

		TEST_METHOD( PinfuTsumoIsTwentyFu )
		{
			const ScoreResult r = ScoreClosed( "234567m234567p11s", BaseContext( true, "2m" ) );

			Assert::IsTrue( Holds( r, Yaku::PINFU ) );
			Assert::IsTrue( Holds( r, Yaku::MENZEN_TSUMO ) );
			Assert::AreEqual( 20, static_cast<int>( r.fu ) );
			Assert::AreEqual( 2, static_cast<int>( r.han ) );
		}

		TEST_METHOD( Tanyao )
		{
			const ScoreResult r = ScoreClosed( "234567m234567p22s", BaseContext( false, "2m" ) );

			Assert::IsTrue( Holds( r, Yaku::TANYAO ) );
			Assert::IsTrue( Holds( r, Yaku::PINFU ) );
			Assert::AreEqual( 2, static_cast<int>( r.han ) );
		}

		TEST_METHOD( YakuhaiOnTheRoundWind )
		{
			// East triplet in an east round, held by the south seat.
			const ScoreResult r = ScoreClosed( "234567m234p11s111z", BaseContext( false, "2m" ) );

			Assert::IsTrue( Holds( r, Yaku::YAKUHAI_ROUND ) );
			Assert::IsFalse( Holds( r, Yaku::YAKUHAI_SEAT ), L"east is not this seat's wind" );
			Assert::IsFalse( Holds( r, Yaku::PINFU ), L"a yakuhai triplet rules out pinfu" );
		}

		TEST_METHOD( SevenPairsIsTwoHanTwentyFiveFu )
		{
			const ScoreResult r = ScoreClosed( "1133557799m1133p", BaseContext( false, "3p" ) );

			Assert::IsTrue( Holds( r, Yaku::CHIITOITSU ) );
			Assert::AreEqual( 2, static_cast<int>( r.han ) );
			Assert::AreEqual( 25, static_cast<int>( r.fu ) );
		}

		TEST_METHOD( Ittsuu )
		{
			const ScoreResult r = ScoreClosed( "123456789m11123p", BaseContext( false, "1p" ) );

			Assert::IsTrue( Holds( r, Yaku::ITTSUU ) );
			Assert::IsTrue( Holds( r, Yaku::PINFU ) );
			Assert::AreEqual( 3, static_cast<int>( r.han ), L"ittsuu is two closed, plus pinfu" );
		}

		TEST_METHOD( SanshokuDoujun )
		{
			const ScoreResult r = ScoreClosed( "123m123p123777s55z", BaseContext( false, "3s" ) );
			Assert::IsTrue( Holds( r, Yaku::SANSHOKU_DOUJUN ) );
		}

		TEST_METHOD( ToitoiAndSanankou )
		{
			// Ron on the 5s, so that triplet was never concealed: three
			// concealed triplets rather than four.
			const ScoreResult r = ScoreClosed( "111999m333p555s77z", BaseContext( false, "5s" ) );

			Assert::IsTrue( Holds( r, Yaku::TOITOI ) );
			Assert::IsTrue( Holds( r, Yaku::SANANKOU ) );
			Assert::IsFalse( Holds( r, Yaku::SUUANKOU ) );
		}

		TEST_METHOD( HonitsuAndChinitsu )
		{
			const ScoreResult honitsu = ScoreClosed( "11122345678999m", BaseContext( false, "9m" ) );
			Assert::IsTrue( Holds( honitsu, Yaku::CHINITSU ) || Holds( honitsu, Yaku::CHUUREN ) );

			const ScoreResult mixed = ScoreClosed( "123456789m11177z", BaseContext( false, "7z" ) );
			Assert::IsTrue( Holds( mixed, Yaku::HONITSU ) );
			Assert::IsFalse( Holds( mixed, Yaku::CHINITSU ) );
		}

		TEST_METHOD( ADoraOnlyHandIsNotAWin )
		{
			// A complete shape with nothing to claim: open, all simples barred
			// by a terminal pair, no yakuhai, no shape yaku.
			Meld melds[3];
			melds[0].type = MeldType::CHI; melds[0].base = Id( RiichiMahjongTile::MAN_2 );
			melds[1].type = MeldType::CHI; melds[1].base = Id( RiichiMahjongTile::PIN_5 );
			melds[2].type = MeldType::CHI; melds[2].base = Id( RiichiMahjongTile::SOU_3 );

			WinContext ctx = BaseContext( false, "8s" );
			const ScoreResult r = ScoreHand( CountsOf( "678s11m" ), melds, 3, ctx );

			Assert::IsFalse( r.valid, L"a complete shape with no yaku is not a win" );
			Assert::AreEqual( 0, r.points );
		}

		TEST_METHOD( DoraAloneNeverCreatesAWin )
		{
			Meld melds[3];
			melds[0].type = MeldType::CHI; melds[0].base = Id( RiichiMahjongTile::MAN_2 );
			melds[1].type = MeldType::CHI; melds[1].base = Id( RiichiMahjongTile::PIN_5 );
			melds[2].type = MeldType::CHI; melds[2].base = Id( RiichiMahjongTile::SOU_3 );

			WinContext ctx = BaseContext( false, "8s" );
			ctx.doraIndicators[0]  = MakeInstance( Id( RiichiMahjongTile::MAN_9 ) ); // 1m is dora
			ctx.doraIndicatorCount = 1;

			const ScoreResult r = ScoreHand( CountsOf( "678s11m" ), melds, 3, ctx );
			Assert::IsFalse( r.valid, L"dora is not a yaku" );
		}
	};

	TEST_CLASS( Yakuman )
	{
	public:
		TEST_METHOD( ThirteenOrphans )
		{
			const ScoreResult r = ScoreClosed( "119m19p19s1234567z", BaseContext( false, "1m" ) );

			Assert::IsTrue( r.valid );
			Assert::IsTrue( Holds( r, Yaku::KOKUSHI ) );
			Assert::AreEqual( 1, static_cast<int>( r.yakumanCount ) );
			Assert::AreEqual( 32000, r.points, L"a non-dealer yakuman ron" );
		}

		TEST_METHOD( FourConcealedTriplets )
		{
			const ScoreResult r = ScoreClosed( "111999m333p555s77z", BaseContext( true, "5s" ) );

			Assert::IsTrue( Holds( r, Yaku::SUUANKOU ) );
			Assert::AreEqual( 1, static_cast<int>( r.yakumanCount ) );
		}

		TEST_METHOD( AllHonours )
		{
			const ScoreResult r = ScoreClosed( "11122233355566z", BaseContext( false, "5z" ) );
			Assert::IsTrue( Holds( r, Yaku::TSUUIISOU ) );
		}

		TEST_METHOD( AYakumanReplacesTheHanCount )
		{
			// The hand also holds toitoi and yakuhai, but they do not add to a
			// yakuman.
			const ScoreResult r = ScoreClosed( "11122233355566z", BaseContext( false, "5z" ) );

			Assert::IsFalse( Holds( r, Yaku::TOITOI ) );
			Assert::AreEqual( 13, static_cast<int>( r.han ) );
		}

		TEST_METHOD( DealerYakumanIsFortyEightThousand )
		{
			WinContext ctx = BaseContext( false, "1m" );
			ctx.isDealer = true;

			const ScoreResult r = ScoreClosed( "119m19p19s1234567z", ctx );
			Assert::AreEqual( 48000, r.points );
		}
	};

	TEST_CLASS( Payments )
	{
	public:
		TEST_METHOD( TheStandardRonTable )
		{
			// 1 han 30 fu, non-dealer ron.
			const ScoreResult one = ScoreClosed( "234567m234567p11s", BaseContext( false, "2m" ) );
			Assert::AreEqual( 1,    static_cast<int>( one.han ) );
			Assert::AreEqual( 30,   static_cast<int>( one.fu ) );
			Assert::AreEqual( 1000, one.points );

			// 2 han 30 fu, non-dealer ron.
			const ScoreResult two = ScoreClosed( "234567m234567p22s", BaseContext( false, "2m" ) );
			Assert::AreEqual( 2,    static_cast<int>( two.han ) );
			Assert::AreEqual( 2000, two.points );
		}

		TEST_METHOD( TheDealerCollectsHalfAgain )
		{
			WinContext ctx = BaseContext( false, "2m" );
			ctx.isDealer = true;

			const ScoreResult r = ScoreClosed( "234567m234567p11s", ctx );
			Assert::AreEqual( 1500, r.points, L"1 han 30 fu, dealer ron" );
		}

		TEST_METHOD( TsumoSplitsBetweenTheDealerAndTheRest )
		{
			// 2 han 20 fu tsumo: base 320, so 700 from the dealer and 400 each.
			const ScoreResult r = ScoreClosed( "234567m234567p11s", BaseContext( true, "2m" ) );

			Assert::AreEqual( 2,  static_cast<int>( r.han ) );
			Assert::AreEqual( 20, static_cast<int>( r.fu ) );
			Assert::AreEqual( 700, r.fromDealer );
			Assert::AreEqual( 400, r.fromEachOther );
			Assert::AreEqual( 1500, r.points, L"700 plus two lots of 400" );
		}

		TEST_METHOD( ADealerTsumoTakesTheSameFromEveryone )
		{
			WinContext ctx = BaseContext( true, "2m" );
			ctx.isDealer = true;

			const ScoreResult r = ScoreClosed( "234567m234567p11s", ctx );

			Assert::AreEqual( 700, r.fromEachOther );
			Assert::AreEqual( 2100, r.points );
		}

		TEST_METHOD( HonbaAndSticksRideOnTop )
		{
			WinContext ctx = BaseContext( false, "2m" );
			ctx.honba        = 2;
			ctx.riichiSticks = 1;

			const ScoreResult r = ScoreClosed( "234567m234567p11s", ctx );

			// 1000 for the hand, 600 for two honba, 1000 for the stick.
			Assert::AreEqual( 1600, r.fromDiscarder );
			Assert::AreEqual( 2600, r.points );
		}

		TEST_METHOD( HonbaOnATsumoIsPerPlayer )
		{
			WinContext ctx = BaseContext( true, "2m" );
			ctx.honba = 1;

			const ScoreResult r = ScoreClosed( "234567m234567p11s", ctx );

			Assert::AreEqual( 800, r.fromDealer );
			Assert::AreEqual( 500, r.fromEachOther );
		}

		TEST_METHOD( ManganCapsTheBaseValue )
		{
			// Five han is mangan however the fu falls.
			WinContext ctx = BaseContext( false, "1p" );
			ctx.doraIndicators[0]  = MakeInstance( Id( RiichiMahjongTile::MAN_9 ) );
			ctx.doraIndicators[1]  = MakeInstance( Id( RiichiMahjongTile::MAN_9 ) );
			ctx.doraIndicatorCount = 2;

			// Ittsuu and pinfu, plus two dora on the single 1m.
			const ScoreResult r = ScoreClosed( "123456789m11123p", ctx );

			Assert::AreEqual( 2, static_cast<int>( r.doraHan ) );
			Assert::AreEqual( 5, static_cast<int>( r.han ) );
			Assert::AreEqual( 8000, r.points, L"five han is mangan whatever the fu" );
		}
	};

	TEST_CLASS( DoraCounting )
	{
	public:
		TEST_METHOD( IndicatorsAkaAndNukiAllCount )
		{
			WinContext ctx = BaseContext( false, "2m" );
			ctx.doraIndicators[0]  = MakeInstance( Id( RiichiMahjongTile::MAN_1 ) ); // 2m is dora
			ctx.doraIndicatorCount = 1;
			ctx.akaCount           = 1;

			const ScoreResult r = ScoreClosed( "234567m234567p11s", ctx );

			// One 2m in the hand, plus the red five.
			Assert::AreEqual( 2, static_cast<int>( r.doraHan ) );
			Assert::AreEqual( 3, static_cast<int>( r.han ), L"pinfu plus two dora" );
		}

		TEST_METHOD( UraOnlyCountsForARiichiHand )
		{
			WinContext ctx = BaseContext( false, "2m" );
			ctx.uraIndicators[0]  = MakeInstance( Id( RiichiMahjongTile::MAN_1 ) );
			ctx.uraIndicatorCount = 1;

			const ScoreResult without = ScoreClosed( "234567m234567p11s", ctx );
			Assert::AreEqual( 0, static_cast<int>( without.doraHan ), L"no riichi, no ura" );

			ctx.riichi = true;
			const ScoreResult with = ScoreClosed( "234567m234567p11s", ctx );
			Assert::AreEqual( 1, static_cast<int>( with.doraHan ) );
		}

		TEST_METHOD( SanmaNukidoraCountsAsDora )
		{
			WinContext ctx = BaseContext( false, "2m" );
			ctx.rules     = MahjongSoul3P();
			ctx.nukiCount = 2;

			const ScoreResult r = ScoreClosed( "234567m234567p11s", ctx );
			Assert::AreEqual( 2, static_cast<int>( r.doraHan ), L"each pulled North is a dora" );
		}
	};

	TEST_CLASS( ScoringThroughTheEngine )
	{
	public:
		TEST_METHOD( AHandWithNoYakuCannotBeDeclared )
		{
			Engine engine( MahjongSoul4P(), 700 );
			engine.StartHand( 0 );

			GameState &s = engine.State();

			// An open hand of runs with a terminal pair: complete, but nothing
			// to claim.
			SetHand( s.players[0], "678s11m" );
			s.players[0].melds[0].type = MeldType::CHI;
			s.players[0].melds[0].base = Id( RiichiMahjongTile::MAN_2 );
			s.players[0].melds[1].type = MeldType::CHI;
			s.players[0].melds[1].base = Id( RiichiMahjongTile::PIN_5 );
			s.players[0].melds[2].type = MeldType::CHI;
			s.players[0].melds[2].base = Id( RiichiMahjongTile::SOU_3 );
			s.players[0].meldCount = 3;
			s.players[0].drawn = MakeInstance( Id( RiichiMahjongTile::SOU_8 ) );

			ActionList legal;
			LegalActions( s, 0, legal );

			Assert::IsFalse( legal.ContainsType( ActionType::TSUMO ),
				L"a complete shape with no yaku is not a declarable win" );
			Assert::IsTrue( engine.Step( Action::Tsumo( 0 ) ) == StepResult::ILLEGAL );
		}

		TEST_METHOD( AClosedTsumoAlwaysHasAYaku )
		{
			Engine engine( MahjongSoul4P(), 701 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			SetHand( s.players[0], WINNING_14 );
			s.players[0].drawn = MakeInstance( Id( RiichiMahjongTile::PIN_1 ) );

			ActionList legal;
			LegalActions( s, 0, legal );
			Assert::IsTrue( legal.ContainsType( ActionType::TSUMO ), L"menzen tsumo is itself a yaku" );
		}

		TEST_METHOD( TheContextIsReadOffTheLiveState )
		{
			Engine engine( MahjongSoul4P(), 702 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			SetHand( s.players[0], WINNING_14 );
			s.players[0].drawn = MakeInstance( Id( RiichiMahjongTile::PIN_1 ) );
			s.players[0].riichiDeclared = true;
			s.honba = 3;

			engine.Step( Action::Tsumo( 0 ) );

			const WinContext ctx = MakeWinContext( s, 0 );

			Assert::IsTrue( ctx.byTsumo );
			Assert::IsTrue( ctx.riichi );
			Assert::IsTrue( ctx.isDealer, L"seat 0 dealt the hand" );
			Assert::AreEqual( 0, static_cast<int>( ctx.seatWind ) );
			Assert::AreEqual( 3, static_cast<int>( ctx.honba ) );
			Assert::AreEqual( 1, static_cast<int>( ctx.doraIndicatorCount ) );

			const ScoreResult r = ScoreHand( s.players[0].hand.Counts(), s.players[0].melds,
				s.players[0].meldCount, ctx );

			Assert::IsTrue( r.valid );
			Assert::IsTrue( Holds( r, Yaku::RIICHI ) );
			Assert::IsTrue( Holds( r, Yaku::MENZEN_TSUMO ) );
			Assert::IsTrue( Holds( r, Yaku::ITTSUU ) );
		}
	};
}
