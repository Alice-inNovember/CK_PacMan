#include "AI/GhostGreen.h"
#include <algorithm>
#include <cmath>
#include "AI/AStar.h"

namespace pacman {
	namespace {
		struct Prediction
		{
			NodeId goal;
			std::vector<NodeId> route;
			double distance;
		};

		// 플레이어 진행 방향의 다음 교차점을 예측한다.
		std::optional<Prediction> nextJunction(const AiContext& context)
		{
			const auto& graph = context.graph;
			const auto& mover = context.player.mover();
			auto current = mover.planningNode();
			std::optional<NodeId> previous = mover.nextNode() ? std::optional{mover.currentNode()} : std::nullopt;
			std::vector<NodeId> route{current};
			std::vector<bool> visited(graph.nodes().size());
			double length = mover.remainingPixels();
			constexpr std::array<TilePos, 4> delta{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
			while (!visited[current.value]) {
				visited[current.value] = true;
				if (previous && graph.isJunction(current))
					return Prediction{current, route, length};
				std::optional<NodeId> next;
				if (!previous) {
					const auto d = delta[static_cast<std::size_t>(context.player.direction())];
					const auto candidate = graph.inDirection(current, d);
					if (candidate && graph.canEnter(*candidate, Traveler::player))
						next = candidate;
				} else {
					const auto d = graph.edgeDelta(*previous, current);
					const auto candidate =
						graph.inDirection(current, {d.x == 0 ? 0 : (d.x > 0 ? 1 : -1), d.y == 0 ? 0 : (d.y > 0 ? 1 : -1)});
					if (candidate && graph.canEnter(*candidate, Traveler::player))
						next = candidate;
				}
				if (!next)
					return {};
				length += graph.edgeCost(current, *next) * Map::tileSize;
				previous = current;
				current = *next;
				route.push_back(current);
			}
			return {};
		}

		// 고스트가 예측 교차점에 먼저 도착할 수 있는지 확인한다.
		bool canReachInterceptFirst(const AiContext& context, const Prediction& prediction,
									std::optional<std::pair<NodeId, NodeId>> blocked)
		{
			const auto& mover = context.actor.mover;
			const auto path = findPath(context.graph, mover.planningNode(), prediction.goal, Traveler::ghost, {}, blocked);
			if (!path)
				return false;
			int tiles = 0;
			for (std::size_t n = 1; n < path->size(); ++n)
				tiles += context.graph.edgeCost((*path)[n - 1], (*path)[n]);
			const double ghostDistance = mover.remainingPixels() + tiles * Map::tileSize;
			return ghostDistance / GameConfig::ghostSpeed + 0.3 <= prediction.distance / Player::speed;
		}
	}	 // namespace

	// 무작위 순찰하는 그린 고스트를 만든다.
	GhostGreen::GhostGreen() : Ghost(GhostStateKind::patrol) {}

	// 순찰 행동의 시간을 갱신한다.
	void GhostGreen::advancePatrol(const AiContext&, double dt)
	{
		mEvaluation -= dt;
	}

	// 순찰 상태의 목표를 결정한다.
	GhostTarget GhostGreen::patrolTarget(const AiContext& context)
	{
		return mPatrol.planTarget(context);
	}

	// 시야를 잃었을 때 추격 기억을 정리한다.
	void GhostGreen::onSightLost()
	{
		mIntercept.reset();
		mPredictedRoute.clear();
	}

	// 상태 전환에 따른 행동 기억을 정리한다.
	void GhostGreen::onStateChanged()
	{
		mPatrol.clearGoal();
		onSightLost();
		mGoalAge = mWaitAge = 0;
	}

	// 색상별 행동 기억을 초기화한다.
	void GhostGreen::resetBehavior()
	{
		mPatrol.reset();
		onStateChanged();
		mRejectedIntercept.reset();
		mLastPlayer.reset();
		mEvaluation = mStillAge = 0;
		mValidatedEdge.reset();
		mValidatedFrom.reset();
		mValidatedTarget.reset();
		mInterceptPathValid = false;
	}

	// 추격 행동의 시간을 갱신한다.
	void GhostGreen::advanceChase(const AiContext& context, double dt)
	{
		const auto position = context.player.mover().position(context.graph);
		if (mLastPlayer && std::hypot(position.x - mLastPlayer->x, position.y - mLastPlayer->y) < 1e-6)
			mStillAge += dt;
		else
			mStillAge = 0;
		mLastPlayer = position;
		mGoalAge += dt;
		mEvaluation -= dt;
		const auto center = context.actor.mover.centerNode();
		if (center && mIntercept) {
			if (center == mIntercept)
				mWaitAge += dt;
			else
				mWaitAge = 0;
		}
	}

	// 플레이어가 보일 때의 추격 목표를 결정한다.
	GhostTarget GhostGreen::chaseVisible(const AiContext& context)
	{
		const auto player = context.player.mover().planningNode();
		const auto& actor = context.actor;
		const auto& mover = actor.mover;
		if (!mover.centerNode())
			return {actor.target, actor.allowed, actor.blockedEdge};
		const auto blocked =
			context.player.mover().nextNode() ? std::optional{std::pair{context.player.mover().currentNode(), player}} : std::nullopt;
		if (mIntercept) {
			const bool leftRoute = std::find(mPredictedRoute.begin(), mPredictedRoute.end(), player) == mPredictedRoute.end();
			if (leftRoute || context.player.mover().centerNode() == mIntercept || mStillAge > 1 || mWaitAge > 1) {
				mRejectedIntercept = mIntercept;
				mIntercept.reset();
				mGoalAge = 0;
			}
		}
		if (mEvaluation <= 0) {
			mEvaluation = 0.25;
			if ((!mIntercept || mGoalAge >= 1) && mStillAge <= 1) {
				const auto prediction = nextJunction(context);
				if (prediction && prediction->goal != mRejectedIntercept && prediction->goal != mIntercept &&
					canReachInterceptFirst(context, *prediction, blocked)) {
					mGoalAge = 0;
					mWaitAge = 0;
					mIntercept = prediction->goal;
					mPredictedRoute = prediction->route;
					mValidatedFrom = mover.currentNode();
					mValidatedTarget = mIntercept;
					mValidatedEdge = blocked;
					mInterceptPathValid = true;
				}
			}
		}

		if (mIntercept) {
			if (mover.currentNode() != mValidatedFrom || mIntercept != mValidatedTarget || blocked != mValidatedEdge ||
				!mInterceptPathValid) {
				mValidatedFrom = mover.currentNode();
				mValidatedTarget = mIntercept;
				mValidatedEdge = blocked;
				mInterceptPathValid = findPath(context.graph, mover.planningNode(), *mIntercept, Traveler::ghost, {}, blocked).has_value();
			}
			if (mInterceptPathValid)
				return {mIntercept, {}, blocked};
			mIntercept.reset();
		}
		return {chaseAttachment(context), {}, {}};
	}

}	 // namespace pacman
