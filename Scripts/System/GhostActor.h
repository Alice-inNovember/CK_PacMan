#pragma once
#include "System/GameConfig.h"
#include "System/Mover.h"

namespace pacman {
	enum class GhostCondition
	{
		normal,
		frightened,
		eaten
	};

	struct GhostTarget
	{
		std::optional<NodeId> node;
		std::vector<bool> allowed;
		std::optional<std::pair<NodeId, NodeId>> blockedEdge;
	};

	using GhostTargets = std::array<GhostTarget, 3>;

	struct GhostActor
	{
		GhostActor(GhostKind ghostKind, Mover mover)
			: kind(ghostKind), mover(std::move(mover))
		{
		}

		std::optional<NodeId> nextNode(const Map&, const NavigationGraph&, const Mover& player,
									   bool returning, NodeId current,
									   const GhostTarget& requested);

		GhostKind kind;
		Mover mover;
		GhostCondition condition = GhostCondition::normal;
		std::optional<NodeId> target;
		std::vector<NodeId> path;
		std::size_t pathSearches = 0;
		std::uint64_t relocation = 0;
		bool routeUnreachable = false;
		std::vector<bool> allowed;
		std::optional<std::pair<NodeId, NodeId>> blockedEdge;

	   private:
		void planPath(const NavigationGraph&, NodeId current, const GhostTarget& requested);
	};

}
