#include "System/GhostActor.h"
#include <algorithm>
#include "AI/AStar.h"
#include "AI/GhostState.h"
namespace pacman {
	// 고스트의 목표 경로를 재사용하거나 새로 탐색한다.
	void GhostActor::planPath(const NavigationGraph& graph, NodeId current, const GhostTarget& requested)
	{
		const auto nextTarget = requested.node;
		const auto& nextAllowed = requested.allowed;
		const auto nextBlocked = requested.blockedEdge;
		const bool routeValid = !nextTarget || std::find(path.begin(), path.end(), current) != path.end();
		if (nextTarget == target && nextAllowed == allowed && nextBlocked == blockedEdge && routeValid)
			return;
		target = nextTarget;
		allowed = nextAllowed;
		blockedEdge = nextBlocked;
		path.clear();
		routeUnreachable = false;
		if (!target)
			return;
		++pathSearches;
		const auto planned = findPath(graph, current, *target, Traveler::ghost, allowed, blockedEdge);
		if (planned)
			path = *planned;
		else
			routeUnreachable = true;
	}

	// 현재 조건과 목표에 따라 다음 이동 노드를 선택한다.
	std::optional<NodeId> GhostActor::nextNode(const Map& map, const NavigationGraph& graph, const Mover& player,
											   bool returning, NodeId current, const GhostTarget& requested)
	{
		returning = returning || condition == GhostCondition::eaten;
		if (!returning && condition == GhostCondition::frightened)
			return FrightenedState::chooseNext(graph, player, current);
		if (returning)
			planPath(graph, current, EatenState::homeTarget(map, graph));
		else
			planPath(graph, current, requested);
		const auto at = std::find(path.begin(), path.end(), current);
		if (at == path.end() || at + 1 == path.end())
			return {};
		return *(at + 1);
	}
}	 // namespace pacman
