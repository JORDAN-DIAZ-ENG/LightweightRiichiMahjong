#pragma once

#include "Core.h"

namespace LRMahjong
{
	class LRM_API Engine
	{
	public:
		Engine() = default;
		virtual ~Engine() = default;

		void Run();
	};

	Engine *CreateEngine(); // To be defined in user's application

} // namespace LRMahjong