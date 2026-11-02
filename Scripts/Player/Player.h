#pragma once

#include "System/Mover.h"

namespace pacman {

	enum class Direction
	{
		up,
		right,
		down,
		left
	};

	class Player
	{
	   public:
		static constexpr double speed = 128;
		Player(const NavigationGraph& graph, NodeId start);

		void queueDirection(Direction direction) { mQueued = direction; }

		void clearQueuedDirection() { mQueued.reset(); }

		bool stopped() const { return mStopped; }

		Direction direction() const { return mDirection; }

		std::optional<Direction> queuedDirection() const { return mQueued; }

		const Mover& mover() const { return mMover; }

		MovementTrace update(const NavigationGraph& graph, double dt);

	   private:
		std::optional<NodeId> chooseNext(const NavigationGraph& graph, NodeId current);
		Mover mMover;
		Direction mDirection = Direction::right;
		std::optional<Direction> mQueued;
		bool mStopped = false;
	};

}
