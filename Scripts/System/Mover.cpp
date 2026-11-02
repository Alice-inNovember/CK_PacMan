#include "System/Mover.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace pacman {
	namespace {
		// 타일의 월드 중심 좌표를 구한다.
		WorldPosition center(const NavigationGraph& graph, NodeId id)
		{
			const auto& node = graph.at(id);
			return {node.centerX, node.centerY};
		}

		// 두 위치 사이의 보간 위치를 구한다.
		WorldPosition interpolate(WorldPosition a, WorldPosition b, double fraction)
		{
			return {a.x + (b.x - a.x) * fraction, a.y + (b.y - a.y) * fraction};
		}

		// 타일과 연결 노드 사이의 거리를 구한다.
		double attachmentDistance(const std::vector<NodeAttachment>& ends, const std::vector<double>& nodes)
		{
			double distance = std::numeric_limits<double>::infinity();
			for (const auto& end : ends)
				distance = std::min(distance, nodes[end.node.value] + end.tiles);
			return distance;
		}
	}	 // namespace

	// 월드 위치를 맵 경계 안으로 보정한다.
	WorldPosition wrapPosition(WorldPosition p)
	{
		constexpr double width = Map::width * Map::tileSize, height = Map::height * Map::tileSize;
		p.x = std::fmod(std::fmod(p.x, width) + width, width);
		p.y = std::fmod(std::fmod(p.y, height) + height, height);
		return p;
	}

	// 경계 순환을 고려한 위치 차이를 반환한다.
	WorldPosition wrappedDelta(WorldPosition from, WorldPosition to)
	{
		return {std::remainder(to.x - from.x, Map::width * Map::tileSize),
				std::remainder(to.y - from.y, Map::height * Map::tileSize)};
	}

	// 기록한 이동 구간에서 해당 시각의 위치를 구한다.
	WorldPosition MovementTrace::positionAt(double time) const
	{
		if (!std::isfinite(time))
			throw std::invalid_argument("interpolation time must be finite");
		time = std::clamp(time, 0.0, duration);
		for (const auto& segment : segments) {
			if (time <= segment.end) {
				const double length = segment.end - segment.begin;
				return interpolate(segment.from, segment.to, length > 0 ? (time - segment.begin) / length : 1);
			}
		}
		return segments.empty() ? initial : segments.back().to;
	}

	// 시작 노드와 통행자로 이동 상태를 만든다.
	Mover::Mover(const NavigationGraph& graph, NodeId start, Traveler traveler) : mCurrent(start), mTraveler(traveler)
	{
		if (!graph.canEnter(start, traveler))
			throw std::invalid_argument("mover start is forbidden for traveler");
	}

	// 현재 위치에서 각 노드까지의 거리를 구한다.
	std::vector<double> Mover::distancesToNodes(const NavigationGraph& graph) const
	{
		const auto& from = graph.distancesFrom(mCurrent);
		const auto& to = graph.distancesFrom(planningNode());
		std::vector<double> result(from.size());
		for (std::size_t i = 0; i < result.size(); ++i)
			result[i] = std::min(from[i] + mProgress / Map::tileSize,
								 to[i] + (mNext ? (mLength - mProgress) / Map::tileSize : 0));
		return result;
	}

	// 현재 위치에서 타일까지의 이동 거리를 구한다.
	double Mover::distanceToTile(const NavigationGraph& graph, TilePos tile,
								 const std::vector<double>& nodeDistances) const
	{
		const auto& ends = graph.attachments(tile);
		double distance = attachmentDistance(ends, nodeDistances);
		if (mNext && ends.size() == 2 &&
			((ends[0].node == mCurrent && ends[1].node == *mNext) ||
			 (ends[1].node == mCurrent && ends[0].node == *mNext))) {
			const auto offset = ends[0].node == mCurrent ? ends[0].tiles : ends[1].tiles;
			distance = std::min(distance, std::abs(mProgress / Map::tileSize - offset));
		}
		return distance;
	}

	// 현재 간선의 이동 방향을 반전한다.
	bool Mover::reverse()
	{
		if (!mNext)
			return false;
		std::swap(mCurrent, *mNext);
		mProgress = mLength - mProgress;
		return true;
	}

	// 그래프에서 현재 연속 위치를 구한다.
	WorldPosition Mover::position(const NavigationGraph& graph) const
	{
		const auto from = center(graph, mCurrent);
		if (!mNext)
			return from;
		const auto d = graph.edgeDelta(mCurrent, *mNext);
		return wrapPosition(
			{from.x + d.x * Map::tileSize * mProgress / mLength, from.y + d.y * Map::tileSize * mProgress / mLength});
	}

	// 노드 중심에 있으면 해당 노드를 반환한다.
	std::optional<NodeId> Mover::centerNode() const
	{
		return mNext ? std::nullopt : std::optional{mCurrent};
	}

	// 타일 중심에 있으면 해당 타일을 반환한다.
	std::optional<TilePos> Mover::tileCenter(const NavigationGraph& graph) const
	{
		const auto pos = position(graph);
		const double x = pos.x / Map::tileSize - 0.5, y = pos.y / Map::tileSize - 0.5;
		if (std::abs(x - std::round(x)) > 1e-8 || std::abs(y - std::round(y)) > 1e-8)
			return {};
		return TilePos{static_cast<int>(std::round(x)), static_cast<int>(std::round(y))};
	}

	// The graph must retain the topology used at construction.
	// chooseNext is called only at centers. Invalid/nonadjacent choices stop movement.
	MovementTrace Mover::advance(const NavigationGraph& graph, double speed, double dt, const NextNode& chooseNext)
	{
		if (!std::isfinite(speed) || speed < 0 || !std::isfinite(dt) || dt < 0 || !std::isfinite(speed * dt))
			throw std::invalid_argument("movement speed and duration must be finite and nonnegative");
		MovementTrace trace{position(graph), dt, {}, {}};
		double remaining = speed * dt;
		double elapsed = 0;
		while (remaining > 0) {
			if (!mNext) {
				const auto choice = chooseNext(mCurrent);
				const auto& neighbors = graph.at(mCurrent).neighbors;
				if (!choice || std::find(neighbors.begin(), neighbors.end(), *choice) == neighbors.end() ||
					!graph.canEnter(*choice, mTraveler))
					break;
				mNext = choice;
				mLength = graph.edgeCost(mCurrent, *mNext) * Map::tileSize;
			}
			const auto from = position(graph);
			double boundary = std::min(mLength, (std::floor(mProgress / Map::tileSize + 1e-9) + 1) * Map::tileSize);
			const bool wrap = graph.isWrapEdge(mCurrent, *mNext);
			const auto delta = graph.edgeDelta(mCurrent, *mNext);
			if (wrap && mProgress < mLength / 2 - 1e-9)
				boundary = mLength / 2;
			const double distance = boundary - mProgress;
			const bool arrives = remaining >= distance || distance - remaining < 1e-9;
			const double moved = arrives ? distance : remaining;
			const double end = std::min(dt, elapsed + moved / speed);
			mProgress += moved;
			remaining = std::max(0.0, remaining - moved);
			if (arrives && mLength - mProgress < 1e-8) {
				mCurrent = *mNext;
				mNext.reset();
				mProgress = 0;
			}
			auto segmentFrom = from;
			auto segmentTo = position(graph);
			if (wrap) {
				segmentTo = {from.x + delta.x * moved, from.y + delta.y * moved};
				// The second half starts on the opposite map boundary, without a swept teleport.
				if (delta.x > 0 && from.x == 0)
					segmentFrom.x = 0;
				if (delta.x < 0 && from.x == 0) {
					segmentFrom.x = Map::width * Map::tileSize;
					segmentTo.x += Map::width * Map::tileSize;
				}
				if (delta.y < 0 && from.y == 0) {
					segmentFrom.y = Map::height * Map::tileSize;
					segmentTo.y += Map::height * Map::tileSize;
				}
			}
			trace.segments.push_back({segmentFrom, segmentTo, elapsed, end});
			elapsed = end;
			if (arrives && (!wrap || !mNext)) {
				const auto tile = tileCenter(graph).value();
				trace.arrivals.push_back({tile, end});
			}
		}
		if (elapsed < dt)
			trace.segments.push_back({position(graph), position(graph), elapsed, dt});
		return trace;
	}
	// 현재 위치에서 각 노드까지의 거리 내림값을 구한다.
	std::vector<int> Mover::flooredDistancesToNodes(const NavigationGraph& graph) const
	{
		const auto exact = distancesToNodes(graph);
		std::vector<int> result(exact.size());
		for (std::size_t i = 0; i < result.size(); ++i)
			result[i] = static_cast<int>(std::floor(exact[i]));
		return result;
	}

	// 현재 위치에서 노드까지의 거리 내림값을 구한다.
	int Mover::flooredDistanceToNode(const NavigationGraph& graph, NodeId node) const
	{
		const auto& from = graph.distancesFrom(mCurrent);
		const auto& to = graph.distancesFrom(planningNode());
		const double distance = std::min(from[node.value] + mProgress / Map::tileSize,
										 to[node.value] + (mNext ? (mLength - mProgress) / Map::tileSize : 0));
		return static_cast<int>(std::floor(distance));
	}

}	 // namespace pacman
