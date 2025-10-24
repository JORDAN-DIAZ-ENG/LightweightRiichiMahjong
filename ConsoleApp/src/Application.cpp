#include <LRMahjong.h>

class CommandLineExample : LRMahjong::Engine
{
public:

};

LRMahjong::Engine * LRMahjong::CreateEngine()
{
	return new LRMahjong::Engine();
}