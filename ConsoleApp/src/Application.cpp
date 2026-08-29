#include <chrono>
#include <iostream>

#include <LRMahjong.h>

using namespace LRMahjong;
using namespace LRMahjong::Model;

namespace
{
	void PrintBanner()
	{
		std::cout << R"(
    __    _       __    __                _       __    __
   / /   (_)___ _/ /_  / /__      _____  (_)___ _/ /_  / /_
  / /   / / __ `/ __ \/ __/ | /| / / _ \/ / __ `/ __ \/ __/
 / /___/ / /_/ / / / / /_ | |/ |/ /  __/ / /_/ / / / / /_
/_____/_/\__, /_/ /_/\__/ |__/|__/\___/_/\__, /_/ /_/\__/
    ____/____/     __    _    __  ___   /____/     _
   / __ \(_|_)____/ /_  (_)  /  |/  /___ _/ /_    (_)___  ____  ____ _
  / /_/ / / / ___/ __ \/ /  / /|_/ / __ `/ __ \  / / __ \/ __ \/ __ `/
 / _, _/ / / /__/ / / / /  / /  / / /_/ / / / / / / /_/ / / / / /_/ /
/_/ |_/_/_/\___/_/ /_/_/  /_/  /_/\__,_/_/ /_/_/ /\____/_/ /_/\__, /
                                            /___/            /____/
    )" << std::endl;
	}

	const char *OutcomeName( const HandOutcome outcome )
	{
		switch ( outcome )
		{
		case HandOutcome::TSUMO:             return "tsumo";
		case HandOutcome::RON:               return "ron";
		case HandOutcome::EXHAUSTIVE_DRAW:   return "exhaustive draw";
		case HandOutcome::ABORT_KYUUSHU:     return "abort: nine terminals";
		case HandOutcome::ABORT_FOUR_KAN:    return "abort: four kans";
		case HandOutcome::ABORT_FOUR_RIICHI: return "abort: four riichi";
		case HandOutcome::ABORT_FOUR_WINDS:  return "abort: four winds";
		case HandOutcome::ABORT_TRIPLE_RON:  return "abort: triple ron";
		default:                             return "none";
		}
	}

	// Discards whatever was just drawn, which is always legal. Enough to drive
	// a hand from the deal to its natural end.
	void PlayOutTsumogiri( Engine &engine )
	{
		int guard = 0;

		while ( engine.State().phase != Phase::HAND_OVER && guard++ < 4000 )
		{
			const GameState &s = engine.State();

			if ( s.phase == Phase::DISCARD )
			{
				engine.Step( Action::Discard( s.currentPlayer, s.players[s.currentPlayer].drawn ) );
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

	// Which seat, if any, currently owes the engine a decision.
	uint8_t SeatToAct( const GameState &s )
	{
		if ( s.phase == Phase::DISCARD ) return s.currentPlayer;

		if ( s.phase == Phase::CALL )
		{
			for ( uint8_t seat = 0; seat < s.PlayerCount(); ++seat )
			{
				if ( ( s.pendingCallers & ( 1u << seat ) ) != 0 ) return seat;
			}
		}

		return INVALID_SEAT;
	}

	// Plays a whole hand by picking uniformly among the legal actions. A
	// crude policy, but it exercises every branch of the generator and would
	// deadlock immediately if any reachable state offered nothing to do.
	void PlayOutRandomly( Engine &engine )
	{
		ActionList legal;
		int guard = 0;

		while ( engine.State().phase != Phase::HAND_OVER && guard++ < 4000 )
		{
			const uint8_t seat = SeatToAct( engine.State() );
			if ( seat == INVALID_SEAT ) break;

			if ( LegalActions( engine.State(), seat, legal ) == 0 ) break;

			engine.Step( legal[static_cast<uint8_t>( engine.Random().Below( legal.count ) )] );
		}
	}

	void ShowUkeire( const char *tenhou )
	{
		Counts34 hand{};
		AkaMask  aka = AKA_NONE;
		if ( !CountsFromTenhouString( tenhou, hand, aka ) ) return;

		// Nothing seen but the hand itself.
		Counts34 remaining{};
		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) remaining[t] = static_cast<uint8_t>( 4 - hand[t] );

		const UkeireResult result = UkeireAgainst( hand, 0, remaining );

		std::cout << tenhou << "\n";
		std::cout << "    shanten : " << static_cast<int>( result.shanten ) << "\n";
		std::cout << "    accepts :";

		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t )
		{
			if ( ( result.tiles & ( 1ULL << t ) ) != 0 ) std::cout << " " << TileToString( t );
		}

		std::cout << "  (" << result.copies << " tiles)\n\n";
	}

	void BenchmarkShanten()
	{
		// A spread of real-ish hands rather than one shape, so the timing is not
		// dominated by whichever branch happens to be cheapest.
		constexpr int HANDS = 20000;

		Rng rng( 12345 );
		Counts34 hands[256];

		for ( auto &hand : hands )
		{
			Engine engine( MahjongSoul4P(), rng.NextU64() );
			engine.StartHand( 0 );
			hand = engine.State().players[0].hand.Counts();
		}

		int checksum = 0;
		const auto start = std::chrono::steady_clock::now();

		for ( int i = 0; i < HANDS; ++i )
		{
			checksum += Shanten( hands[i % 256], 0 );
		}

		const auto elapsed = std::chrono::steady_clock::now() - start;
		const double us = std::chrono::duration<double, std::micro>( elapsed ).count();

		std::cout << "Shanten: " << HANDS << " calls in " << us << " us ("
			<< ( us / HANDS ) << " us each, "
			<< static_cast<long long>( HANDS / ( us / 1e6 ) ) << " per second)\n";

		// Keeps the loop from being optimised away.
		if ( checksum == INT32_MIN ) std::cout << "";
	}

	void ShowScore( const char *hand, const char *winningTile, const bool tsumo )
	{
		Counts34 counts{};
		AkaMask  aka = AKA_NONE;
		if ( !CountsFromTenhouString( hand, counts, aka ) ) return;

		WinContext ctx;
		ctx.rules       = MahjongSoul4P();
		ctx.byTsumo     = tsumo;
		ctx.winningTile = MakeInstance( TileFromTenhouString( winningTile ) );
		ctx.seatWind    = 1; // south seat
		ctx.roundWind   = 0; // east round

		const ScoreResult r = ScoreHand( counts, nullptr, 0, ctx );

		std::cout << hand << "  " << ( tsumo ? "tsumo" : "ron" ) << " on " << winningTile << "\n";

		if ( !r.valid )
		{
			std::cout << "    no yaku, so not a win\n\n";
			return;
		}

		std::cout << "    yaku    :";
		for ( uint8_t bit = 0; bit < static_cast<uint8_t>( Yaku::COUNT ); ++bit )
		{
			if ( ( r.yaku & ( 1ULL << bit ) ) != 0 ) std::cout << " " << YakuName( static_cast<Yaku>( bit ) );
		}
		std::cout << "\n";

		std::cout << "    han/fu  : " << static_cast<int>( r.han ) << " han " << static_cast<int>( r.fu ) << " fu";
		if ( r.yakumanCount > 0 ) std::cout << "  (yakuman x" << static_cast<int>( r.yakumanCount ) << ")";
		std::cout << "\n";

		std::cout << "    points  : " << r.points;
		if ( tsumo ) std::cout << "  (" << r.fromEachOther << " each, " << r.fromDealer << " from the dealer)";
		std::cout << "\n\n";
	}

	// Plays part of a hand, then shows what one seat could work out about it.
	void ShowBelief( const uint64_t seed, const uint8_t viewer, const int turns )
	{
		Engine engine( MahjongSoul4P(), seed );
		engine.EnableEventLog( true );
		engine.StartHand( 0 );

		int played = 0;
		while ( engine.State().phase != Phase::HAND_OVER && played < turns )
		{
			const GameState &s = engine.State();

			if ( s.phase == Phase::DISCARD )
			{
				engine.Step( Action::Discard( s.currentPlayer, s.players[s.currentPlayer].drawn ) );
				++played;
			}
			else
			{
				for ( uint8_t seat = 0; seat < s.PlayerCount(); ++seat )
				{
					if ( ( s.pendingCallers & ( 1u << seat ) ) != 0 ) engine.Step( Action::Pass( seat ) );
				}
			}
		}

		const Observation obs = engine.ViewFor( viewer );

		Belief belief;
		belief.BuildFrom( obs );

		int unseenTotal = 0;
		for ( TileId t = 0; t < TILE_KIND_COUNT; ++t ) unseenTotal += belief.unseen[t];

		std::cout << "Seat " << static_cast<int>( viewer ) << " after " << turns << " discards\n";
		std::cout << "    events seen  : " << obs.eventCount << "\n";
		std::cout << "    own hand     : " << belief.publicState.players[viewer].hand.ToTenhouString() << "\n";
		std::cout << "    tiles unseen : " << unseenTotal << "\n";
		std::cout << "    opponents    :";
		for ( uint8_t seat = 0; seat < 4; ++seat )
		{
			if ( seat == viewer ) continue;
			std::cout << " seat" << static_cast<int>( seat ) << "=" << static_cast<int>( belief.hands[seat].size );
		}
		std::cout << " tiles\n";

		// Three sampled worlds, to show the same evidence admits many.
		Rng rng( seed ^ 0x5eed );
		for ( int sample = 0; sample < 3; ++sample )
		{
			GameState world;
			if ( !Determinize( belief, rng, world ) )
			{
				std::cout << "    sample " << sample << "   : no consistent world found\n";
				continue;
			}

			const uint8_t other = static_cast<uint8_t>( ( viewer + 1 ) % 4 );
			std::cout << "    sample " << sample << "     : seat" << static_cast<int>( other ) << " could hold "
				<< world.players[other].hand.ToTenhouString() << "\n";
		}

		std::cout << "\n";
	}

	void RunHand( const char *label, const Rules &rules, const uint64_t seed )
	{
		Engine engine( rules, seed );
		engine.StartHand( 0 );

		std::cout << label << "  (seed " << seed << ")\n";
		std::cout << "    dora indicator : "
			<< TileToString( InstanceTile( engine.State().DoraIndicator( 0 ) ) ) << "\n";
		std::cout << "    dealt hand     : " << engine.State().players[0].hand.ToTenhouString() << "\n";

		PlayOutTsumogiri( engine );

		const GameState &s = engine.State();

		int discards = 0;
		for ( uint8_t seat = 0; seat < s.PlayerCount(); ++seat ) discards += s.players[seat].discardCount;

		std::cout << "    outcome        : " << OutcomeName( s.result.outcome ) << "\n";
		std::cout << "    tiles drawn    : " << discards << "\n";
		std::cout << "    wall remaining : " << static_cast<int>( s.LiveWallRemaining() ) << "\n\n";
	}
}

int main()
{
	PrintBanner();

	std::cout << "GameState size: " << sizeof( GameState ) << " bytes\n\n";

	RunHand( "Mahjong Soul, four player: ", MahjongSoul4P(), 20250824 );
	RunHand( "Mahjong Soul, three player:", MahjongSoul3P(), 20250824 );

	// The same seed must replay the same hand, or no rollout is debuggable.
	Engine a( MahjongSoul4P(), 4242 );
	Engine b( MahjongSoul4P(), 4242 );
	a.StartHand( 0 );
	b.StartHand( 0 );
	PlayOutTsumogiri( a );
	PlayOutTsumogiri( b );

	bool identical = true;
	for ( uint8_t seat = 0; seat < 4 && identical; ++seat )
	{
		const Player &pa = a.State().players[seat];
		const Player &pb = b.State().players[seat];

		if ( pa.discardCount != pb.discardCount ) identical = false;

		for ( uint8_t i = 0; i < pa.discardCount && identical; ++i )
		{
			if ( pa.discards[i] != pb.discards[i] ) identical = false;
		}
	}

	std::cout << "Seeded replay is deterministic: " << ( identical ? "yes" : "no" ) << "\n\n";

	// A random-legal policy over many hands: every branch of the generator
	// gets exercised, and any reachable state with no legal action would show
	// up here as a hang rather than as a subtle bug much later.
	constexpr int HANDS = 500;

	int outcomes[9] = {};
	const auto start = std::chrono::steady_clock::now();

	for ( int i = 0; i < HANDS; ++i )
	{
		Engine engine( MahjongSoul4P(), static_cast<uint64_t>( i ) );
		engine.StartHand( static_cast<uint8_t>( i % 4 ) );
		PlayOutRandomly( engine );

		++outcomes[static_cast<int>( engine.Result().outcome )];
	}

	const auto elapsed = std::chrono::steady_clock::now() - start;
	const double ms = std::chrono::duration<double, std::milli>( elapsed ).count();

	std::cout << HANDS << " hands under a random legal policy, " << ms << " ms ("
		<< ( ms / HANDS ) << " ms per hand)\n";

	for ( int i = 1; i < 9; ++i )
	{
		if ( outcomes[i] == 0 ) continue;
		std::cout << "    " << OutcomeName( static_cast<HandOutcome>( i ) ) << ": " << outcomes[i] << "\n";
	}

	std::cout << "\n";
	ShowUkeire( "123456789m1123p" );
	ShowUkeire( "13m456789m11123p" );
	ShowUkeire( "147m258p369s1234z" );

	ShowScore( "234567m234567p11s", "2m", false );
	ShowScore( "123456789m11123p", "1p", true );
	ShowScore( "119m19p19s1234567z", "1m", false );

	ShowBelief( 20260824, 0, 12 );

	BenchmarkShanten();

	return 0;
}
