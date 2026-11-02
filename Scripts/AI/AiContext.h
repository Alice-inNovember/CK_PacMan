#pragma once
#include "System/RoundState.h"

namespace pacman {
	struct GhostObservation
	{
		const Mover& mover;
		GhostCondition condition;
		const std::optional<NodeId>& target;
		const std::vector<bool>& allowed;
		const std::optional<std::pair<NodeId, NodeId>>& blockedEdge;
		std::uint64_t relocation;
	};

	struct AiContext
	{
		const Map& map;
		const NavigationGraph& graph;
		const Player& player;
		GhostObservation actor;
		RoundPhase phase;
		bool released;
		std::size_t index;
	};
}
