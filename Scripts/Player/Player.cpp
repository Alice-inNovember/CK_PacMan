#include "Player/Player.h"
#include <cmath>

namespace pacman {

	// 시작 노드에서 플레이어를 초기화한다.
	Player::Player(const NavigationGraph& graph, NodeId start) : mMover(graph, start, Traveler::player) {}

	// 다음 이동 노드를 선택한다.
	std::optional<NodeId> Player::chooseNext(const NavigationGraph& graph, NodeId current)
	{
		constexpr std::array<TilePos, 4> directionDeltas{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

		const auto neighbor = [&](Direction direction) -> std::optional<NodeId> {
			const auto delta = directionDeltas[static_cast<std::size_t>(direction)];
			const auto node = graph.inDirection(current, delta);
			return node && graph.canEnter(*node, Traveler::player) ? node : std::nullopt;
		};
		if (mQueued) {
			const auto requested = *mQueued;
			mQueued.reset();
			if (const auto next = neighbor(requested)) {
				mDirection = requested;
				mStopped = false;
				return next;
			}
			mStopped = true;
			return std::nullopt;
		}
		if (!mStopped) {
			if (const auto next = neighbor(mDirection))
				return next;
			mStopped = true;
		}
		return std::nullopt;
	}

	// 시간에 따라 상태를 갱신한다.
	MovementTrace Player::update(const NavigationGraph& graph, double dt)
	{
		if (dt > 0 && std::isfinite(dt) && mQueued && (static_cast<int>(*mQueued) + 2) % 4 == static_cast<int>(mDirection) &&
			mMover.reverse()) {
			mDirection = *mQueued;
			mQueued.reset();
			mStopped = false;
		}
		return mMover.advance(graph, speed, dt, [&](NodeId current) { return chooseNext(graph, current); });
	}
}	 // namespace pacman
