#include "AI/AStar.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <queue>

namespace pacman {
	// A successful path includes both endpoints. nullopt means unreachable.
	std::optional<std::vector<NodeId>> findPath(const NavigationGraph& graph, NodeId start, NodeId goal, Traveler traveler,
												const std::vector<bool>& allowed, std::optional<std::pair<NodeId, NodeId>> blockedEdge)
	{
		if (!allowed.empty() && allowed.size() != graph.nodes().size())
			throw std::invalid_argument("path region size mismatch");
		if (!graph.canEnter(start, traveler) || !graph.canEnter(goal, traveler))
			return std::nullopt;
		if (!allowed.empty() && (!allowed[start.value] || !allowed[goal.value]))
			return std::nullopt;

		struct Entry
		{
			int f;
			NodeId node;

			bool operator>(const Entry& other) const { return f != other.f ? f > other.f : node.value > other.node.value; }
		};

		const auto heuristic = [&](NodeId id) {
			const auto a = graph.at(id).tile;
			const auto b = graph.at(goal).tile;
			const int dx = std::abs(a.x - b.x), dy = std::abs(a.y - b.y);
			return std::min(dx, Map::width - dx) + std::min(dy, Map::height - dy);
		};
		std::vector<int> cost(graph.nodes().size(), std::numeric_limits<int>::max());
		std::vector<bool> closed(graph.nodes().size());
		std::vector<std::optional<NodeId>> parent(graph.nodes().size());
		std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
		cost[start.value] = 0;
		open.push({heuristic(start), start});
		while (!open.empty()) {
			const auto current = open.top().node;
			open.pop();
			if (closed[current.value])
				continue;
			closed[current.value] = true;
			if (current == goal) {
				std::vector<NodeId> path{goal};
				while (path.back() != start)
					path.push_back(parent[path.back().value].value());
				std::reverse(path.begin(), path.end());
				return path;
			}
			for (auto next : graph.at(current).neighbors) {
				if ((!allowed.empty() && !allowed[next.value]) ||
					(blockedEdge && ((current == blockedEdge->first && next == blockedEdge->second) ||
									 (current == blockedEdge->second && next == blockedEdge->first))) ||
					!graph.canEnter(next, traveler) || closed[next.value])
					continue;
				const int candidate = cost[current.value] + graph.edgeCost(current, next);
				if (candidate < cost[next.value]) {
					cost[next.value] = candidate;
					parent[next.value] = current;
					open.push({candidate + heuristic(next), next});
				}
			}
		}
		return std::nullopt;
	}
}	 // namespace pacman
