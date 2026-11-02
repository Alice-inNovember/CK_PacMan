#include "AI/PatrolBehavior.h"
#include <algorithm>
#include <cstdlib>

namespace pacman {
	// 현재 관측에 따라 행동과 전이를 평가한다.
	void PatrolBehavior::observe(const AiContext& context)
	{
		const auto& graph = context.graph;
		const auto center = std::optional{context.actor.mover.currentNode()};
		if (center != mLastCenter) {
			mPrevious = mLastCenter;
			mLastCenter = center;
			if (graph.isJunction(*center)) {
				mRecent.push_back(*center);
				if (mRecent.size() > 4)
					mRecent.pop_front();
			}
		}
	}

	// 현재 상태에 맞는 이동 목표를 결정한다.
	GhostTarget PatrolBehavior::planTarget(const AiContext& context)
	{
		const auto& graph = context.graph;
		const auto center = context.actor.mover.centerNode();
		if (!center) {
			const auto& actor = context.actor;
			return {actor.target, actor.allowed, actor.blockedEdge};
		}
		const auto& mover = context.actor.mover;
		if (mGoal && mover.centerNode() != mGoal)
			return {mGoal, {}, {}};
		const auto current = mover.planningNode();
		auto options = graph.at(current).neighbors;
		const auto home = graph.nodeAt(context.map.house()).value();
		if (options.size() > 1)
			std::erase(options, home);
		if (options.size() > 1 && mPrevious)
			std::erase(options, *mPrevious);
		if (options.empty())
			return {};
		int total = 0;
		for (const auto node : options)
			total += std::find(mRecent.begin(), mRecent.end(), node) == mRecent.end() ? 3 : 1;
		int choice = std::rand() % total;
		for (const auto node : options) {
			choice -= std::find(mRecent.begin(), mRecent.end(), node) == mRecent.end() ? 3 : 1;
			if (choice < 0) {
				mGoal = node;
				break;
			}
		}
		return {mGoal, {}, {}};
	}

	// 내부 상태를 초기화한다.
	void PatrolBehavior::reset()
	{
		mGoal.reset();
		mPrevious.reset();
		mLastCenter.reset();
		mRecent.clear();
	}
}	 // namespace pacman
