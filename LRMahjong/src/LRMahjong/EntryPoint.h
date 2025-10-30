#pragma once

#ifdef LRM_PLATFORM_WINDOWS


int main( int argc, char** argv )
{
	auto engine = LRMahjong::Engine(); // Create engine on the stack
	engine.Start();
}

#endif // LRM_PLATFORM_WINDOWS