#include "AI/NavigationGraph.h"
#include <cstdlib>
#include <limits>
#include <queue>

namespace pacman {
	namespace {
		// 타일에서 이동 가능한 방향을 구한다.
		std::vector<TilePos> walkableDirections(const Map& map, TilePos here)
		{
			std::vector<TilePos> neighbors;
			for (auto d : std::array<TilePos, 4>{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}})
				if (map.step(here, d))
					neighbors.push_back(d);
			return neighbors;
		}
	}	 // namespace

	// 타일에 해당하는 탐색 노드를 찾는다.
	std::optional<NodeId> NavigationGraph::nodeAt(TilePos tile) const
	{
		if (!Map::contains(tile))
			return std::nullopt;
		return mTileNodes[static_cast<std::size_t>(tile.y * Map::width + tile.x)];
	}

	// 타일에서 연결되는 노드와 거리를 반환한다.
	const std::vector<NodeAttachment>& NavigationGraph::attachments(TilePos tile) const
	{
		if (!Map::contains(tile))
			throw std::out_of_range("attachment tile outside map");
		return mAttachments[static_cast<std::size_t>(tile.y * Map::width + tile.x)];
	}

	// 통행자가 노드에 진입할 수 있는지 확인한다.
	bool NavigationGraph::canEnter(NodeId id, Traveler traveler) const
	{
		at(id);
		return traveler == Traveler::ghost || id != mHouse;
	}

	// 간선이 맵 경계를 통과하는지 확인한다.
	bool NavigationGraph::isWrapEdge(NodeId from, NodeId to) const
	{
		const auto a = at(from).tile, b = at(to).tile;
		return (a.y == b.y && std::abs(a.x - b.x) == Map::width - 1) || (a.x == b.x && std::abs(a.y - b.y) == Map::height - 1);
	}

	// 간선의 이동 방향과 길이를 구한다.
	TilePos NavigationGraph::edgeDelta(NodeId from, NodeId to) const
	{
		const auto a = at(from).tile, b = at(to).tile;
		TilePos delta{b.x - a.x, b.y - a.y};
		if (isWrapEdge(from, to)) {
			if (delta.x)
				delta.x = delta.x > 0 ? -1 : 1;
			if (delta.y)
				delta.y = delta.y > 0 ? -1 : 1;
		}
		return delta;
	}

	// 간선의 실제 타일 길이를 반환한다.
	int NavigationGraph::edgeCost(NodeId from, NodeId to) const
	{
		const auto d = edgeDelta(from, to);
		return std::abs(d.x) + std::abs(d.y);
	}

	// 지정 방향으로 연결된 노드를 찾는다.
	std::optional<NodeId> NavigationGraph::inDirection(NodeId from, TilePos direction) const
	{
		for (auto next : at(from).neighbors) {
			const auto d = edgeDelta(from, next);
			if ((direction.x && d.y == 0 && d.x * direction.x > 0) || (direction.y && d.x == 0 && d.y * direction.y > 0))
				return next;
		}
		return {};
	}

	// 기준 노드의 가중 거리장을 반환한다.
	const std::vector<int>& NavigationGraph::distancesFrom(NodeId source) const
	{
		if (const auto cached = mDistanceCache.find(source.value); cached != mDistanceCache.end())
			return cached->second;
		std::vector<int> distance(mNodes.size(), std::numeric_limits<int>::max());
		using Entry = std::pair<int, std::size_t>;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
		distance.at(source.value) = 0;
		open.push({0, source.value});
		while (!open.empty()) {
			const auto [cost, index] = open.top();
			open.pop();
			if (cost != distance[index])
				continue;
			for (auto next : at(NodeId{index}).neighbors) {
				const int candidate = cost + edgeCost(NodeId{index}, next);
				if (candidate < distance[next.value]) {
					distance[next.value] = candidate;
					open.push({candidate, next.value});
				}
			}
		}
		return mDistanceCache.emplace(source.value, std::move(distance)).first->second;
	}

	// 직선 통로를 따라 다음 노드까지 연결한다.
	void NavigationGraph::connectCorridor(const Map& map, NodeId from, TilePos delta)
	{
		auto& node = mNodes[from.value];
		auto cursor = map.step(node.tile, delta);
		int tiles = 1;
		while (cursor) {
			if (auto next = nodeAt(*cursor)) {
				node.neighbors.push_back(*next);
				break;
			}
			mAttachments[cursor->y * Map::width + cursor->x].push_back({from, tiles});
			cursor = map.step(*cursor, delta);
			++tiles;
		}
	}

	// 맵에서 압축 탐색 그래프를 생성한다.
	NavigationGraph NavigationGraph::build(const Map& map)
	{
		NavigationGraph graph;
		for (int y = 0; y < Map::height; ++y) {
			for (int x = 0; x < Map::width; ++x) {
				if (!map.isWalkable({x, y}))
					continue;
				const TilePos here{x, y};
				const auto neighbors = walkableDirections(map, here);
				const bool straight = neighbors.size() == 2 && neighbors[0].x + neighbors[1].x == 0 && neighbors[0].y + neighbors[1].y == 0;
				const bool wrapAnchor = ((x <= 1 || x >= Map::width - 2) && map.step({0, y}, {-1, 0})) ||
										((y <= 1 || y >= Map::height - 2) && map.step({x, 0}, {0, -1}));
				const bool anchor = here == map.playerSpawn() || here == map.house() || here == map.houseExit() || wrapAnchor;
				if (straight && !anchor)
					continue;
				NodeId id{graph.mNodes.size()};
				graph.mTileNodes[static_cast<std::size_t>(y * Map::width + x)] = id;
				graph.mNodes.push_back({{x, y}, (x + 0.5f) * Map::tileSize, (y + 0.5f) * Map::tileSize, {}});
			}
		}
		constexpr std::array<TilePos, 4> directions{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
		for (std::size_t n = 0; n < graph.mNodes.size(); ++n) {
			auto& node = graph.mNodes[n];
			graph.mAttachments[node.tile.y * Map::width + node.tile.x].push_back({NodeId{n}, 0});
			for (auto delta : directions) {
				graph.connectCorridor(map, NodeId{n}, delta);
			}
		}
		graph.mHouse = graph.nodeAt(map.house()).value();
		return graph;
	}

}	 // namespace pacman
