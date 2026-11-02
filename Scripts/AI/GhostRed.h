#pragma once
#include "AI/Ghost.h"

namespace pacman {
	class GhostRed final : public Ghost
	{
	   public:
		GhostRed();

	   private:
		GhostTarget patrolTarget(const AiContext&) override;
		GhostTarget chaseVisible(const AiContext&) override;
	};
}
