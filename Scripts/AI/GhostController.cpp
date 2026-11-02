#include "AI/GhostController.h"
#include "AI/GhostBlue.h"
#include "AI/GhostGreen.h"
#include "AI/GhostRed.h"
#include "System/Game.h"

namespace pacman {
	// 고스트 팀을 초기화한다.
	GhostController::GhostController()
	{
		reset();
	}

	// 내부 상태를 초기화한다.
	void GhostController::reset()
	{
		mGhosts = {std::make_unique<GhostRed>(), std::make_unique<GhostGreen>(), std::make_unique<GhostBlue>()};
	}

	// 시간을 진행하고 결과를 갱신한다.
	GhostTargets GhostController::advance(const Game& game, double dt)
	{
		GhostTargets targets;
		for (std::size_t i = 0; i < mGhosts.size(); ++i)
			targets[i] = mGhosts[i]->advance(game.aiContext(i), dt);
		return targets;
	}

	// 노드 중심에서 시간을 진행하지 않고 목표를 평가한다.
	GhostTarget GhostController::planAtNode(const AiContext& context)
	{
		return mGhosts.at(context.index)->planAtNode(context);
	}
}	 // namespace pacman
