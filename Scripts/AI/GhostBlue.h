#pragma once
#include "AI/GhostGreen.h"

namespace pacman {
	class GhostBlue final : public GhostGreen
	{
	   public:
		GhostBlue();

	   private:
		void advancePatrol(const AiContext&, double) override;
		void advanceChase(const AiContext&, double) override;
		GhostTarget patrolTarget(const AiContext&) override;
		GhostTarget chaseVisible(const AiContext&) override;
		void onStateChanged() override;
		void resetBehavior() override;
		void clearGuard();
		GhostTarget guardPower(const AiContext&);
		void buildGuardRoute(const AiContext&);
		std::optional<TilePos> mPower;
		std::optional<TilePos> mLastPower;
		std::vector<TilePos> mVisitedPowers;
		std::vector<NodeId> mGuardRoute;
	};
}
