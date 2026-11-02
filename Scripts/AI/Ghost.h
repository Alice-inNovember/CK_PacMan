#pragma once
#include <memory>
#include "AI/GhostState.h"

namespace pacman {
	class Ghost
	{
	   public:
		static bool canSeePosition(const Map&, WorldPosition observer, WorldPosition target);

		virtual ~Ghost() = default;
		GhostTarget advance(const AiContext&, double);
		GhostTarget planAtNode(const AiContext&);
		const char* stateName() const;
		std::optional<NodeId> lastSeenNode() const { return mState->searchMemory().lastSeenNode; }
		double searchWaitRemaining() const { return GameConfig::searchWaitSeconds - mState->searchMemory().waitElapsed; }
		bool searching() const { return mState->searchMemory().searching; }

	   protected:
		Ghost(GhostStateKind initialState, bool alwaysChase = false);

		static NodeId chaseAttachment(const AiContext&);
		virtual void advancePatrol(const AiContext&, double) {}
		virtual void advanceChase(const AiContext&, double) {}
		virtual GhostTarget patrolTarget(const AiContext&) = 0;
		virtual GhostTarget chaseVisible(const AiContext&) = 0;
		virtual void recordMovement(const AiContext&) {}
		virtual void onStateChanged() {}
		virtual void onSightLost() {}
		virtual void resetBehavior() {}

	   private:
		friend class PatrolState;
		friend class ChaseState;
		void applyState(const StateTransition& transition);
		void resetNormalMemory();
		GhostTarget evaluate(const AiContext&, double);
		bool mAlwaysChase;
		GhostStateKind mInitialState;
		std::unique_ptr<GhostState> mState;
		std::uint64_t mLastRelocation = 0;
		bool mRecovering = false;
	};

	using GhostTeam = std::array<std::unique_ptr<Ghost>, 3>;
}
