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

	std::cout << "Seeded replay is deterministic: " << ( identical ? "yes" : "no" ) << "\n";

	return 0;
}
