#include <stdexcept>
#include "AI/Ghost.h"

namespace pacman {
	namespace {
		// 관측한 플레이어가 고스트의 시야에 있는지 확인한다.
		bool visible(const AiContext& context)
		{
			return Ghost::canSeePosition(context.map, context.actor.mover.position(context.graph),
										 context.player.mover().position(context.graph));
		}
	}	 // namespace

	// 현재 관측에 따라 행동과 전이를 평가한다.
	std::optional<StateTransition> PatrolState::observe(Ghost& ghost, const AiContext& context, double dt)
	{
		if (ghost.mAlwaysChase)
			return StateTransition{GhostStateKind::chase, {}};
		if (visible(context))
			return StateTransition{GhostStateKind::chase, context.player.mover().planningNode()};
		ghost.advancePatrol(context, dt);
		return {};
	}

	// 현재 상태에 맞는 이동 목표를 결정한다.
	GhostTarget PatrolState::planTarget(Ghost& ghost, const AiContext& context)
	{
		return ghost.patrolTarget(context);
	}

	// 최초 관측 노드로 추격 상태를 초기화한다.
	ChaseState::ChaseState(std::optional<NodeId> observedNode)
	{
		mSearch.lastSeenNode = observedNode;
	}

	// 현재 관측에 따라 행동과 전이를 평가한다.
	std::optional<StateTransition> ChaseState::observe(Ghost& ghost, const AiContext& context, double dt)
	{
		if (ghost.mAlwaysChase) {
			ghost.advanceChase(context, dt);
			return {};
		}
		if (!visible(context)) {
			mSearch.searching = true;
			ghost.onSightLost();
			if (!mSearch.lastSeenNode)
				return StateTransition{GhostStateKind::patrol, {}};
			if (context.actor.mover.centerNode() == mSearch.lastSeenNode) {
				mSearch.waitElapsed += dt;
				if (mSearch.waitElapsed + 1e-10 >= GameConfig::searchWaitSeconds)
					return StateTransition{GhostStateKind::patrol, {}};
			}
			return {};
		}
		mSearch.lastSeenNode = context.player.mover().planningNode();
		mSearch.waitElapsed = 0;
		mSearch.searching = false;
		ghost.advanceChase(context, dt);
		return {};
	}

	// 현재 상태에 맞는 이동 목표를 결정한다.
	GhostTarget ChaseState::planTarget(Ghost& ghost, const AiContext& context)
	{
		if (mSearch.searching)
			return {mSearch.lastSeenNode, {}, {}};
		return ghost.chaseVisible(context);
	}

	// 다음 이동 노드를 선택한다.
	std::optional<NodeId> FrightenedState::chooseNext(const NavigationGraph& graph, const Mover& player, NodeId current)
	{
		std::optional<NodeId> best;
		int bestDistance = 0;
		for (auto neighbor : graph.at(current).neighbors) {
			const int distance = player.flooredDistanceToNode(graph, neighbor);
			if (!best || distance > bestDistance) {
				best = neighbor;
				bestDistance = distance;
			}
		}
		return best;
	}

	// 고스트 하우스의 복귀 목표를 반환한다.
	GhostTarget EatenState::homeTarget(const Map& map, const NavigationGraph& graph)
	{
		return {graph.nodeAt(map.house()), {}, {}};
	}

	// 전이 정보에 맞는 고스트 상태를 생성한다.
	std::unique_ptr<GhostState> createGhostState(const StateTransition& transition)
	{
		switch (transition.kind) {
			case GhostStateKind::patrol:
				return std::make_unique<PatrolState>();
			case GhostStateKind::chase:
				return std::make_unique<ChaseState>(transition.observedNode);
			case GhostStateKind::frightened:
				return std::make_unique<FrightenedState>();
			case GhostStateKind::eaten:
				return std::make_unique<EatenState>();
		}
		throw std::logic_error("Unknown ghost state");
	}
}	 // namespace pacman
