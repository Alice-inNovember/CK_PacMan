#pragma once
#include <deque>
#include "AI/AiContext.h"

namespace pacman {
	class PatrolBehavior
	{
	   public:
		void observe(const AiContext&);
		GhostTarget planTarget(const AiContext&);

		void clearGoal() { mGoal.reset(); }

		void reset();

	   private:
		std::optional<NodeId> mPrevious, mLastCenter, mGoal;
		std::deque<NodeId> mRecent;
	};
}
