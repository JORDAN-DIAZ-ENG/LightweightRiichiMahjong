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

// Export decorator for symbols that only exist in debug builds.
//
// NOTE: this is a *decorator*, not a feature flag. It expands to nothing in
// release, so `#ifdef LRM_DEBUG_API` is always true and never guards anything.
// Use `#ifdef _DEBUG` when you need conditional compilation.
#ifdef _DEBUG
	#define LRM_DEBUG_API LRM_API
#else
	#define LRM_DEBUG_API
#endif

// Classes are deliberately *not* exported wholesale. Only the individual
// functions that cross the DLL boundary carry LRM_API, so that the hot paths
// (shanten, ukeire, rollouts) stay inlineable in the consumer and we avoid
// C4251 on standard-library members.
