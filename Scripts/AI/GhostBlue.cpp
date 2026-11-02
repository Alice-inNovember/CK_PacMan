#include "AI/GhostBlue.h"
#include <algorithm>

namespace pacman {
	namespace {
		// 플레이어와 가까운 일반 파워 후보를 구한다.
		std::vector<TilePos> nearestPowers(const AiContext& context)
		{
			const auto& player = context.player.mover();
			const auto distances = player.distancesToNodes(context.graph);
			std::vector<std::pair<double, TilePos>> powers;
			for (int y = 0; y < Map::height; ++y)
				for (int x = 0; x < Map::width; ++x) {
					const TilePos tile{x, y};
					if (context.map.at(tile).collectible == Collectible::powerPellet)
						powers.push_back({player.distanceToTile(context.graph, tile, distances), tile});
				}
			std::stable_sort(powers.begin(), powers.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
			std::vector<TilePos> result;
			for (std::size_t i = 0; i < std::min<std::size_t>(3, powers.size()); ++i)
				result.push_back(powers[i].second);
			return result;
		}
	}	 // namespace

	// 파워를 순찰하는 블루 고스트를 만든다.
	GhostBlue::GhostBlue() : GhostGreen() {}

	// 블루의 파워 순찰 계획을 지운다.
	void GhostBlue::clearGuard()
	{
		mPower.reset();
		mLastPower.reset();
		mVisitedPowers.clear();
		mGuardRoute.clear();
	}

	// 상태 전환에 따른 행동 기억을 정리한다.
	void GhostBlue::onStateChanged()
	{
		GhostGreen::onStateChanged();
		clearGuard();
	}

	// 색상별 행동 기억을 초기화한다.
	void GhostBlue::resetBehavior()
	{
		GhostGreen::resetBehavior();
		clearGuard();
	}

	// 순찰 행동의 시간을 갱신한다.
	void GhostBlue::advancePatrol(const AiContext& context, double dt)
	{
		if (context.map.remainingPowerPellets() <= 1)
			GhostGreen::advancePatrol(context, dt);
	}

	// 추격 행동의 시간을 갱신한다.
	void GhostBlue::advanceChase(const AiContext& context, double dt)
	{
		if (context.map.remainingPowerPellets() <= 1)
			GhostGreen::advanceChase(context, dt);
	}

	// 순찰 상태의 목표를 결정한다.
	GhostTarget GhostBlue::patrolTarget(const AiContext& context)
	{
		if (context.map.remainingPowerPellets() <= 1) {
			clearGuard();
			return GhostGreen::patrolTarget(context);
		}
		return guardPower(context);
	}

	// 플레이어가 보일 때의 추격 목표를 결정한다.
	GhostTarget GhostBlue::chaseVisible(const AiContext& context)
	{
		if (context.map.remainingPowerPellets() <= 1)
			return GhostGreen::chaseVisible(context);
		return {chaseAttachment(context), {}, {}};
	}

	// 파워 주변의 순찰 노드를 구성한다.
	void GhostBlue::buildGuardRoute(const AiContext& context)
	{
		const auto& graph = context.graph;
		const auto home = graph.nodeAt(context.map.house()).value();
		mGuardRoute.clear();
		const auto add = [&](NodeId node) {
			if (node != home && std::find(mGuardRoute.begin(), mGuardRoute.end(), node) == mGuardRoute.end())
				mGuardRoute.push_back(node);
		};
		const auto powerNode = graph.nodeAt(*mPower);
		if (powerNode && graph.at(*powerNode).neighbors.size() != 2) {
			add(*powerNode);
			return;
		}
		std::vector<std::pair<NodeId, NodeId>> approaches;
		if (powerNode) {
			for (const auto next : graph.at(*powerNode).neighbors)
				approaches.push_back({*powerNode, next});
		} else {
			const auto& ends = graph.attachments(*mPower);
			for (const auto& end : ends)
				add(end.node);
			// A corridor endpoint may be a corner; continue towards its first junction or dead end.
			if (ends.size() == 2) {
				mGuardRoute.clear();
				approaches.push_back({ends[1].node, ends[0].node});
				approaches.push_back({ends[0].node, ends[1].node});
			}
		}
		for (auto [previous, current] : approaches) {
			std::vector<bool> visited(graph.nodes().size());
			visited[previous.value] = true;
			while (!visited[current.value]) {
				visited[current.value] = true;
				const auto& neighbors = graph.at(current).neighbors;
				if (neighbors.size() != 2)
					break;
				const auto next = neighbors[0] == previous ? neighbors[1] : neighbors[0];
				previous = current;
				current = next;
			}
			if (visited[current.value])
				add(current);
		}
	}

	// 블루가 순찰할 파워와 노드를 선택한다.
	GhostTarget GhostBlue::guardPower(const AiContext& context)
	{
		const auto center = context.actor.mover.centerNode();
		if (!center)
			return {context.actor.target, context.actor.allowed, context.actor.blockedEdge};
		if (mPower && context.map.at(*mPower).collectible != Collectible::powerPellet) {
			mPower.reset();
			mGuardRoute.clear();
		}
		if (!mPower) {
			const auto candidates = nearestPowers(context);
			auto select = [&](bool skipVisited) -> std::optional<TilePos> {
				for (const auto tile : candidates)
					if (tile != mLastPower &&
						(!skipVisited || std::find(mVisitedPowers.begin(), mVisitedPowers.end(), tile) == mVisitedPowers.end()))
						return tile;
				return {};
			};
			mPower = select(true);
			if (!mPower) {
				mVisitedPowers.clear();
				mPower = select(false);
			}
			if (!mPower)
				return mPatrol.planTarget(context);
			buildGuardRoute(context);
			mPatrol.clearGoal();
		}
		std::erase(mGuardRoute, *center);
		if (mGuardRoute.empty()) {
			mLastPower = mPower;
			mVisitedPowers.push_back(*mPower);
			mPower.reset();
			return {};
		}
		const auto& distance = context.graph.distancesFrom(*center);
		const auto target = *std::min_element(mGuardRoute.begin(), mGuardRoute.end(),
											  [&](NodeId a, NodeId b) { return distance[a.value] < distance[b.value]; });
		return {target, {}, {}};
	}
}	 // namespace pacman
