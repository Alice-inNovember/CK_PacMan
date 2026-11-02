#pragma once

#include <functional>
#include <utility>
#include "AI/AiContext.h"
#include "System/GameConfig.h"
#include "System/RoundState.h"

namespace pacman {

	class Game
	{
	   public:
		using NodePlanner = std::function<GhostTarget(const AiContext&)>;
		explicit Game(Map map);
		void restart();
		void addDebugLife();
		AiContext aiContext(std::size_t index) const;
		std::vector<GameEvent> takeEvents();
		void queueDirection(Direction direction);
		void step(const GhostTargets& targets, NodePlanner nodePlanner = {});

		const Map& map() const { return mMap; }

		const NavigationGraph& graph() const { return mGraph; }

		const Player& player() const { return mPlayer; }

		const std::array<GhostActor, 3>& ghosts() const { return mGhosts; }

		RoundPhase phase() const { return mRound.phase; }

		int score() const { return mRound.score; }

		int lives() const { return mRound.lives; }

		double powerRemaining() const { return mRound.powerRemaining; }

		double ghostSpeed(std::size_t index) const;
		bool ghostReleased(std::size_t index) const;

		double recoveryTime() const { return mRound.recoveryTime; }

		const std::string& error() const { return mRound.error; }

		const MovementTrace& playerTrace() const { return mPlayerTrace; }

		const std::array<MovementTrace, 3>& ghostTraces() const { return mGhostTraces; }

	   private:
		struct MovementPlan
		{
			std::array<std::optional<NodeId>, 3> next{};
			double duration;
		};

		MovementPlan planMovement(double left, const GhostTargets& targets);
		double movementHorizon(double left, const std::array<std::optional<NodeId>, 3>& next) const;
		void move(const MovementPlan& plan, double offset);
		void resolveContacts();
		void tick(const GhostTargets& targets, const NodePlanner& nodePlanner);
		void simulate(double dt, const GhostTargets& targets, const NodePlanner& nodePlanner);
		bool ghostActive(std::size_t index) const;
		bool shuffleGhosts();
		void collectAtPlayer();
		bool terminal() const;

		Map mInitialMap;
		Map mMap;
		NavigationGraph mGraph;
		Player mPlayer;
		std::array<GhostActor, 3> mGhosts;
		RoundState mRound;
		std::vector<GameEvent> mEvents;
		MovementTrace mPlayerTrace{};
		std::array<MovementTrace, 3> mGhostTraces{};
	};

}
