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

	void ShowHand( const char *label, const std::string_view tenhou )
	{
		Hand hand;
		if ( !Hand::FromTenhouString( tenhou, hand ) )
		{
			std::cout << label << ": <malformed> \"" << tenhou << "\"\n";
			return;
		}

		std::cout << label << ": " << hand.ToString()
			<< "\n    tenhou   : " << hand.ToTenhouString()
			<< "\n    tiles    : " << static_cast<int>( hand.TotalTiles() )
			<< "\n    aka mask : " << static_cast<int>( hand.Aka() ) << "\n\n";
	}

	void ShowRules( const char *label, const Rules &rules )
	{
		std::cout << label
			<< "\n    players    : " << static_cast<int>( rules.numPlayers )
			<< "\n    tiles      : " << rules.TotalTiles()
			<< "\n    live draws : " << rules.LiveWallTiles()
			<< "\n    chi        : " << ( rules.allowChi ? "yes" : "no" )
			<< "\n    nukidora   : " << ( rules.nukidora ? "yes" : "no" )
			<< "\n    start pts  : " << rules.startingPoints << "\n\n";
	}
}

int main()
{
	PrintBanner();

	std::cout << "GameState size: " << sizeof( GameState ) << " bytes\n\n";

	ShowRules( "Mahjong Soul, four player:", MahjongSoul4P() );
	ShowRules( "Mahjong Soul, three player:", MahjongSoul3P() );

	ShowHand( "Closed hand   ", "123m456p789s11z22z" );
	ShowHand( "With a red five", "1230m456p789s11z" );

	// Two engines on the same seed must produce identical streams.
	Engine a( MahjongSoul4P(), 12345 );
	Engine b( MahjongSoul4P(), 12345 );
	bool identical = true;
	for ( int i = 0; i < 1000; ++i )
	{
		if ( a.Random().NextU64() != b.Random().NextU64() ) identical = false;
	}
	std::cout << "Seeded RNG reproducible: " << ( identical ? "yes" : "no" ) << "\n";

	return 0;
}
