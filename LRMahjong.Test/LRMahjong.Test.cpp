#include "pch.h"
#include "CppUnitTest.h"

#include "LRMahjong.h"
#include <LRMahjong/Model/Tile.h>

using namespace LRMahjong::Model;

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace LRMahjongTest
{
	TEST_CLASS( TileHelperSanityTest )
	{
		Tile tile;

		void TestManzu( RiichiMahjongTile t )
		{
			tile.type = t;
			Assert::IsTrue(  tile.IsManzu() );
			Assert::IsFalse( tile.IsPinzu() );
			Assert::IsFalse( tile.IsSouzu() );
			Assert::IsFalse( tile.IsWind() );
			Assert::IsFalse( tile.IsDragon() );
			Assert::IsFalse( tile.IsHonor() );
		}

		TEST_METHOD( ValidateIsManzu )
		{
			const uint8_t start = static_cast<uint8_t>( RiichiMahjongTile::MAN_1 );
			const uint8_t end   = static_cast<uint8_t>( RiichiMahjongTile::MAN_9 );

			for ( uint8_t i = start; i <= end; ++i )
			{
				tile.type = static_cast<RiichiMahjongTile>( i );
				TestManzu( tile.type );
			}

		}

		void TestPinzu()
		{
			Assert::IsFalse( tile.IsManzu() );
			Assert::IsTrue(  tile.IsPinzu() );
			Assert::IsFalse( tile.IsSouzu() );
			Assert::IsFalse( tile.IsWind() );
			Assert::IsFalse( tile.IsDragon() );
			Assert::IsFalse( tile.IsHonor() );
		}

		TEST_METHOD( ValidateIsPinzu )
		{
			const uint8_t start = static_cast< uint8_t >( RiichiMahjongTile::PIN_1 );
			const uint8_t end = static_cast< uint8_t >( RiichiMahjongTile::PIN_9 );

			for ( uint8_t i = start; i <= end; ++i )
			{
				tile.type = static_cast< RiichiMahjongTile >( i );
				TestPinzu();
			}
		}

		void TestSouzu()
		{
			Assert::IsFalse( tile.IsManzu() );
			Assert::IsFalse( tile.IsPinzu() );
			Assert::IsTrue(  tile.IsSouzu() );
			Assert::IsFalse( tile.IsWind() );
			Assert::IsFalse( tile.IsDragon() );
			Assert::IsFalse( tile.IsHonor() );
		}

		TEST_METHOD( ValidateIsSouzu )
		{
			const uint8_t start = static_cast<uint8_t> ( RiichiMahjongTile::SOU_1 );
			const uint8_t end = static_cast<uint8_t> ( RiichiMahjongTile::SOU_9 );
			for ( uint8_t i = start; i <= end; ++i )
			{
				tile.type = static_cast<RiichiMahjongTile>( i );
				TestSouzu();
			}
		}

		void TestWind()
		{
			Assert::IsFalse( tile.IsManzu() );
			Assert::IsFalse( tile.IsPinzu() );
			Assert::IsFalse( tile.IsSouzu() );
			Assert::IsTrue(  tile.IsWind() );
			Assert::IsFalse( tile.IsDragon() );
			Assert::IsTrue(  tile.IsHonor() );
		}

		TEST_METHOD( ValidateIsWind )
		{
			const uint8_t start = static_cast< uint8_t > ( RiichiMahjongTile::EAST );
			const uint8_t end = static_cast<uint8_t> ( RiichiMahjongTile::NORTH );
			for ( uint8_t i = start; i <= end; ++i )
			{
				tile.type = static_cast< RiichiMahjongTile >( i );
				TestWind();
			}
		}

		void TestDragon()
		{
			Assert::IsFalse( tile.IsManzu() );
			Assert::IsFalse( tile.IsPinzu() );
			Assert::IsFalse( tile.IsSouzu() );
			Assert::IsFalse( tile.IsWind() );
			Assert::IsTrue(  tile.IsDragon() );
			Assert::IsTrue(  tile.IsHonor() );
		}

		TEST_METHOD( ValidateIsDragon )
		{
			const uint8_t start = static_cast< uint8_t > ( RiichiMahjongTile::RED_DRAGON );
			const uint8_t end = static_cast< uint8_t > ( RiichiMahjongTile::GREEN_DRAGON );
			for ( uint8_t i = start; i <= end; ++i )
			{
				tile.type = static_cast< RiichiMahjongTile >( i );
				TestDragon();
			}
		}

		void TestHonor()
		{
			Assert::IsFalse( tile.IsManzu() );
			Assert::IsFalse( tile.IsPinzu() );
			Assert::IsFalse( tile.IsSouzu() );
			Assert::IsTrue( tile.IsWind() || tile.IsDragon() );
			Assert::IsTrue( tile.IsHonor() );
		}

		TEST_METHOD( ValidateIsHonor )
		{
			const uint8_t start = static_cast<uint8_t> ( RiichiMahjongTile::EAST );
			const uint8_t end = static_cast<uint8_t> ( RiichiMahjongTile::GREEN_DRAGON );
			for ( uint8_t i = start; i <= end; ++i )
			{
				tile.type = static_cast< RiichiMahjongTile >( i );
				TestHonor();
			}
		}


	};
}
