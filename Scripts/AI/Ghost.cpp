#include "AI/Ghost.h"
#include <algorithm>
#include <cmath>

namespace pacman {
	// 초기 행동 상태로 고스트를 만든다.
	// 초기 행동 상태로 고스트를 만든다.
	Ghost::Ghost(GhostStateKind initialState, bool alwaysChase)
		: mAlwaysChase(alwaysChase), mInitialState(initialState), mState(createGhostState({initialState, {}}))
	{
	}

	// 플레이어가 있는 통로의 추격 노드를 고른다.
	NodeId Ghost::chaseAttachment(const AiContext& context)
	{
		const auto& player = context.player.mover();
		const auto& ghost = context.actor.mover;
		if (!ghost.centerNode())
			return context.actor.target.value_or(ghost.planningNode());
		if (!player.nextNode())
			return player.currentNode();
		const auto& distance = context.graph.distancesFrom(ghost.planningNode());
		const double back =
			(context.graph.edgeCost(player.currentNode(), player.planningNode()) * Map::tileSize - player.remainingPixels()) /
			Map::tileSize;
		const double forward = player.remainingPixels() / Map::tileSize;
		auto target = distance[player.currentNode().value] + back < distance[player.planningNode().value] + forward ? player.currentNode()
																													: player.planningNode();
		if (target == ghost.planningNode())
			target = target == player.currentNode() ? player.planningNode() : player.currentNode();
		return target;
	}

	// 현재 행동 상태의 이름을 반환한다.
	const char* Ghost::stateName() const
	{
		return mState->name();
	}

	// 요청된 상태 전이를 적용한다.
	void Ghost::applyState(const StateTransition& transition)
	{
		if (mState->kind() == transition.kind)
			return;
		mState = createGhostState(transition);
		onStateChanged();
	}

	// 정상 행동의 관측과 목표 기억을 지운다.
	void Ghost::resetNormalMemory()
	{
		mState->resetMemory();
		resetBehavior();
	}

	// 시간을 진행하고 결과를 갱신한다.
	GhostTarget Ghost::advance(const AiContext& context, double dt)
	{
		return evaluate(context, dt);
	}

	// 노드 중심에서 시간을 진행하지 않고 목표를 평가한다.
	GhostTarget Ghost::planAtNode(const AiContext& context)
	{
		return evaluate(context, 0);
	}

	// 공통 조건과 현재 상태를 평가해 목표를 결정한다.
	GhostTarget Ghost::evaluate(const AiContext& context, double dt)
	{
		const auto& actor = context.actor;
		if (context.phase == RoundPhase::recovering) {
			if (!mRecovering) {
				resetNormalMemory();
				applyState({mInitialState, {}});
			}
			mRecovering = true;
			return {};
		}
		mRecovering = false;
		if (!context.released)
			return {};
		if (mLastRelocation != actor.relocation) {
			resetNormalMemory();
			mLastRelocation = actor.relocation;
		}
		recordMovement(context);
		const auto old = mState->kind();
		const auto desired = actor.condition == GhostCondition::eaten							   ? GhostStateKind::eaten
							 : actor.condition == GhostCondition::frightened					   ? GhostStateKind::frightened
							 : (old == GhostStateKind::eaten || old == GhostStateKind::frightened) ? (mInitialState)
																								   : old;
		if (desired != old) {
			if (desired == GhostStateKind::patrol || desired == GhostStateKind::chase)
				resetNormalMemory();
			// No old-state decision is made when a higher-priority common condition has changed.
			applyState({desired, {}});
		}
		const auto transition = mState->observe(*this, context, dt);
		// Deferred application: the currently executing State has returned before it is destroyed.
		if (transition) {
			applyState(*transition);
			return {};
		}
		return mState->planTarget(*this, context);
	}

	// 벽과 시야 범위를 고려해 위치가 보이는지 확인한다.
	bool Ghost::canSeePosition(const Map& map, WorldPosition b, WorldPosition a)
	{
		const auto delta = wrappedDelta(b, a);
		const double dx = std::abs(delta.x), dy = std::abs(delta.y);
		if (!((dx <= GameConfig::sightAxisTolerance && dy <= GameConfig::sightPixels) ||
			  (dy <= GameConfig::sightAxisTolerance && dx <= GameConfig::sightPixels)))
			return false;
		const int steps = std::max(1, static_cast<int>(std::ceil(std::hypot(dx, dy) / 8)));
		for (int i = 1; i < steps; ++i) {
			const double f = static_cast<double>(i) / steps;
			const auto sample = wrapPosition({b.x + delta.x * f, b.y + delta.y * f});
			TilePos tile{static_cast<int>(sample.x / Map::tileSize), static_cast<int>(sample.y / Map::tileSize)};
			if (!map.isWalkable(tile))
				return false;
		}
		return true;
	}

}	 // namespace pacman
