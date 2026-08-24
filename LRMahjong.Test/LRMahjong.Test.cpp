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
		TEST_METHOD( DiscardOpensTheCallWindow )
		{
			Engine engine( MahjongSoul4P(), 11 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();
			const StepResult r = engine.Step( Action::Discard( 0, s.players[0].drawn ) );

			Assert::IsTrue( r == StepResult::OK );
			Assert::IsTrue( s.phase == Phase::CALL );
			Assert::AreEqual( 14, static_cast<int>( s.pendingCallers ), L"the other three seats may respond" );
			Assert::AreEqual( 1, static_cast<int>( s.players[0].discardCount ) );
			Assert::IsFalse( s.players[0].awaitingDiscard );
			Assert::AreEqual( 0, static_cast<int>( s.lastDiscarder ) );
		}

		TEST_METHOD( PassingAllTheWayAdvancesTheTurn )
		{
			Engine engine( MahjongSoul4P(), 11 );
			engine.StartHand( 0 );

			const GameState &s = engine.State();
			engine.Step( Action::Discard( 0, s.players[0].drawn ) );

			engine.Step( Action::Pass( 1 ) );
			Assert::IsTrue( s.phase == Phase::CALL, L"still waiting on two more seats" );
			engine.Step( Action::Pass( 2 ) );
			engine.Step( Action::Pass( 3 ) );

			Assert::IsTrue( s.phase == Phase::DISCARD );
			Assert::AreEqual( 1, static_cast<int>( s.currentPlayer ) );
			Assert::AreEqual( 14, static_cast<int>( s.players[1].hand.TotalTiles() ) );
			Assert::IsTrue( s.players[1].awaitingDiscard );
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

			engine.Step( Action::Tsumo( 0 ) );
			Assert::IsTrue( engine.State().phase == Phase::HAND_OVER );

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
			const TileInstance thrown = s.players[0].drawn;
			const TileId t = InstanceTile( thrown );

			// Hand sizes are deliberately not kept honest here; this exercises
			// the call mechanics, not the deal.
			s.players[1].hand.Clear();
			s.players[1].hand.Add( t );
			s.players[1].hand.Add( t );

			engine.Step( Action::Discard( 0, thrown ) );
			Assert::IsTrue( engine.Step( Action::Pon( 1, t ) ) == StepResult::OK );

			Assert::AreEqual( 1, static_cast<int>( s.players[1].meldCount ) );
			Assert::IsTrue( s.players[1].melds[0].type == MeldType::PON );
			Assert::AreEqual( static_cast<int>( t ), static_cast<int>( s.players[1].melds[0].base ) );
			Assert::AreEqual( 0, static_cast<int>( s.players[1].hand.TotalTiles() ), L"both copies left the hand" );

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

			for ( uint8_t seat = 1; seat <= 2; ++seat )
			{
				s.players[seat].hand.Clear();
				s.players[seat].hand.Add( Id( RiichiMahjongTile::SOU_1 ) );
				s.players[seat].hand.Add( Id( RiichiMahjongTile::SOU_2 ) );
			}

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

			Assert::IsTrue( engine.Step( Action::Riichi( 0, s.players[0].drawn ) ) == StepResult::OK );

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
			engine.Step( Action::Riichi( 0, s.players[0].drawn ) );
			engine.Step( Action::Pass( 1 ) );
			engine.Step( Action::Pass( 2 ) );
			engine.Step( Action::Pass( 3 ) );

			// Round back to the declarer.
			for ( int i = 0; i < 3; ++i )
			{
				engine.Step( Action::Discard( s.currentPlayer, s.players[s.currentPlayer].drawn ) );
				for ( uint8_t seat = 0; seat < 4; ++seat )
				{
					if ( ( s.pendingCallers & ( 1u << seat ) ) != 0 ) engine.Step( Action::Pass( seat ) );
				}
			}

			Assert::AreEqual( 0, static_cast<int>( s.currentPlayer ) );

			TileId other = INVALID_TILE;
			const TileId drawnTile = InstanceTile( s.players[0].drawn );
			for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
			{
				if ( t != drawnTile && s.players[0].hand.Count( t ) > 0 ) { other = t; break; }
			}
			Assert::IsTrue( IsValidTile( other ) );

			Assert::IsTrue( engine.Step( Action::Discard( 0, MakeInstance( other ) ) ) == StepResult::ILLEGAL );
			Assert::IsTrue( engine.Step( Action::Discard( 0, s.players[0].drawn ) ) == StepResult::OK );
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
			engine.Step( Action::Tsumo( 0 ) );

			Assert::IsTrue( s.result.rinshan );
			Assert::IsFalse( s.result.haitei, L"a dead wall tile is never haitei" );
		}

		TEST_METHOD( HouteiIsWinningOnTheFinalDiscard )
		{
			Engine engine( MahjongSoul4P(), 90 );
			engine.StartHand( 0 );

			GameState &s = engine.State();
			s.liveWallHead = s.liveWallTail;

			engine.Step( Action::Discard( 0, s.players[0].drawn ) );
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
}
