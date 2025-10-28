#pragma once

#ifdef LRM_PLATFORM_WINDOWS
	#ifdef ENGINE_STATIC // For static build configurations (unit testing)
		#define LRM_API
	#else
		#ifdef LRM_BUILD_DLL
			#define LRM_API __declspec(dllexport)
		#else
			#define LRM_API __declspec(dllimport)
		#endif
	#endif
#else
	#error LRM only supports Windows!
#endif