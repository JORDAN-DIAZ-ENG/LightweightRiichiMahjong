#include "pch.h"
#include "CppUnitTest.h"

#include "LRMahjong.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

LRMahjong::Engine* LRMahjong::CreateEngine()
{
	return new LRMahjong::Engine();
}

namespace LRMahjongTest
{

	TEST_CLASS(LRMahjongTest)
	{
	public:
		
		TEST_METHOD(TestMethod1)
		{
		}
	};
}
