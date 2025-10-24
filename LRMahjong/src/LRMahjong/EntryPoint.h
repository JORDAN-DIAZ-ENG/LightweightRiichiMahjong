#pragma once

#ifdef LRM_PLATFORM_WINDOWS

extern LRMahjong::Engine *LRMahjong::CreateEngine();

int main( int argc, char** argv )
{
	auto engine = LRMahjong::CreateEngine();
	engine->Run();
	delete engine;
}

#endif // LRM_PLATFORM_WINDOWS