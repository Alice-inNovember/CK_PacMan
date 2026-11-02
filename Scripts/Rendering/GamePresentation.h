#pragma once
#include "System/RoundState.h"

namespace pacman {
	class Game;
	class GhostController;

	struct ActorPresentation
	{
		WorldPosition position;
		Direction direction;
	};

	struct GhostPresentation
	{
		ActorPresentation actor;
		GhostKind kind;
		GhostCondition condition;
	};

	struct GamePresentation
	{
		GamePresentation(const Game&, double interpolation);

		const Map& map;
		ActorPresentation player;
		std::array<GhostPresentation, 3> ghosts;
		bool playerVisible;
		bool powered;
		int score;
		int lives;
		int remaining;
		double powerRemaining;
		RoundPhase phase;
		std::optional<Direction> queuedDirection;
		const std::string& error;
	};

	struct GhostDebugView
	{
		const Mover& mover;
		const std::vector<NodeId>& path;
		std::optional<NodeId> target;
		std::size_t pathSearches;
		GhostCondition condition;
		bool released;
		const char* stateName;
		std::optional<NodeId> lastSeenNode;
		bool searching;
		double searchWaitRemaining;
	};

	struct DebugView
	{
		DebugView(const Game&, const GhostController&);

		const Map& map;
		const NavigationGraph& graph;
		RoundPhase phase;
		std::array<GhostDebugView, 3> ghosts;
	};

}
