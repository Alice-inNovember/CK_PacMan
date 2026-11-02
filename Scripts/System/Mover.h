#pragma once

#include <functional>
#include "AI/NavigationGraph.h"

namespace pacman {

	struct WorldPosition
	{
		double x;
		double y;
	};

	WorldPosition wrapPosition(WorldPosition position);
	WorldPosition wrappedDelta(WorldPosition from, WorldPosition to);

	struct MovementSegment
	{
		WorldPosition from;
		WorldPosition to;
		double begin;
		double end;
	};

	struct TileArrival
	{
		TilePos tile;
		double time;
	};

	struct MovementTrace
	{
		WorldPosition initial;
		double duration;
		std::vector<MovementSegment> segments;
		std::vector<TileArrival> arrivals;
		WorldPosition positionAt(double time) const;
	};

	class Mover
	{
	   public:
		using NextNode = std::function<std::optional<NodeId>(NodeId)>;

		Mover(const NavigationGraph& graph, NodeId start, Traveler traveler);
		WorldPosition position(const NavigationGraph& graph) const;
		bool reverse();

		double remainingPixels() const { return mNext ? mLength - mProgress : 0; }

		double distanceToTile(const NavigationGraph& graph, TilePos tile,
							  const std::vector<double>& nodeDistances) const;
		std::vector<int> flooredDistancesToNodes(const NavigationGraph& graph) const;
		int flooredDistanceToNode(const NavigationGraph& graph, NodeId node) const;
		std::vector<double> distancesToNodes(const NavigationGraph& graph) const;
		std::optional<NodeId> centerNode() const;
		std::optional<TilePos> tileCenter(const NavigationGraph& graph) const;

		NodeId planningNode() const { return mNext.value_or(mCurrent); }

		std::optional<NodeId> nextNode() const { return mNext; }

		NodeId currentNode() const { return mCurrent; }

		MovementTrace advance(const NavigationGraph& graph, double speed, double dt, const NextNode& chooseNext);

	   private:
		NodeId mCurrent;
		std::optional<NodeId> mNext;
		double mProgress = 0;
		double mLength = Map::tileSize;
		Traveler mTraveler;
	};

}
