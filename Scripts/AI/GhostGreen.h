#pragma once
#include "AI/Ghost.h"
#include "AI/PatrolBehavior.h"

namespace pacman {
	class GhostGreen : public Ghost
	{
	   public:
		GhostGreen();

	   protected:
		PatrolBehavior mPatrol;

		void recordMovement(const AiContext& context) override { mPatrol.observe(context); }

		void advancePatrol(const AiContext&, double) override;
		void advanceChase(const AiContext&, double) override;
		GhostTarget patrolTarget(const AiContext&) override;
		GhostTarget chaseVisible(const AiContext&) override;
		void onStateChanged() override;
		void onSightLost() override;
		void resetBehavior() override;

	   private:
		double mEvaluation = 0, mGoalAge = 0, mWaitAge = 0, mStillAge = 0;
		std::optional<WorldPosition> mLastPlayer;
		std::optional<NodeId> mIntercept, mRejectedIntercept, mValidatedFrom, mValidatedTarget;
		std::optional<std::pair<NodeId, NodeId>> mValidatedEdge;
		bool mInterceptPathValid = false;
		std::vector<NodeId> mPredictedRoute;
	};
}
