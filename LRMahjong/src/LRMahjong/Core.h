#pragma once

#ifdef LRM_PLATFORM_WINDOWS
	#ifdef LRM_BUILD_DLL
		#define LRM_API __declspec(dllexport)
	#else
		#define LRM_API __declspec(dllimport)
	#endif
#else
	#error LRM only supports Windows!
#endif