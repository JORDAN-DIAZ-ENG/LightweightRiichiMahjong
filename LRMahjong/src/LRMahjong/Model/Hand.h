#pragma once


#include <array>
#include "Tile.h"

namespace LRMahjong::Model
{
	using HandArray = std::array<Tile, 14>;
	using TenhouString = std::string;

	class Hand
	{
	public:
		Hand(); // Generates random hand, 0 awareness of tile legality
		Hand( const TenhouString &tenhouString );

		HandArray GetHand( const bool getSorted );

		void PrintHand();
		TenhouString ToTenhouString();

		//Histogram Representation

	private:
		void generateRandomHand();
		void Sort();

		HandArray _sorted;
		HandArray _unsorted;

	};
}


