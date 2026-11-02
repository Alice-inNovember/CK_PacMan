#pragma once

#include <utility>
#include "AI/NavigationGraph.h"

namespace pacman {

	std::optional<std::vector<NodeId>> findPath(const NavigationGraph& graph, NodeId start, NodeId goal,
												Traveler traveler = Traveler::ghost, const std::vector<bool>& allowed = {},
												std::optional<std::pair<NodeId, NodeId>> blockedEdge = {});

}
