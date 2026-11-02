#pragma once

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>
#include "Map/Map.h"

namespace pacman {

	enum class Traveler
	{
		ghost,
		player
	};

	struct NodeId
	{
		std::size_t value;
		bool operator==(const NodeId&) const = default;
	};

	struct NavigationNode
	{
		TilePos tile;
		float centerX;
		float centerY;
		std::vector<NodeId> neighbors;
	};

	struct NodeAttachment
	{
		NodeId node;
		int tiles;
	};

	class NavigationGraph
	{
	   public:
		static NavigationGraph build(const Map& map);

		const std::vector<NavigationNode>& nodes() const { return mNodes; }

		const NavigationNode& at(NodeId id) const { return mNodes.at(id.value); }

		std::optional<NodeId> nodeAt(TilePos tile) const;
		const std::vector<NodeAttachment>& attachments(TilePos tile) const;
		bool canEnter(NodeId id, Traveler traveler) const;
		TilePos edgeDelta(NodeId from, NodeId to) const;
		bool isWrapEdge(NodeId from, NodeId to) const;
		int edgeCost(NodeId from, NodeId to) const;
		std::optional<NodeId> inDirection(NodeId from, TilePos direction) const;
		const std::vector<int>& distancesFrom(NodeId source) const;

		bool isJunction(NodeId id) const { return at(id).neighbors.size() >= 3; }

	   private:
		void connectCorridor(const Map&, NodeId, TilePos);
		mutable std::unordered_map<std::size_t, std::vector<int>> mDistanceCache;
		std::vector<NavigationNode> mNodes;
		NodeId mHouse{};
		std::array<std::vector<NodeAttachment>, Map::width * Map::height> mAttachments{};
		std::array<std::optional<NodeId>, Map::width * Map::height> mTileNodes{};
	};

}
