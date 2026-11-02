#include "Rendering/GamePresentation.h"
#include <algorithm>
#include <cmath>
#include "AI/GhostController.h"
#include "System/Game.h"

namespace pacman {
	// 게임 상태에서 프레임 표시 자료를 구성한다.
	GamePresentation::GamePresentation(const Game& game, double interpolation)
		: map(game.map()),
		  player{game.playerTrace().positionAt(std::clamp(interpolation, 0.0, 1.0) * GameConfig::fixedStep), game.player().direction()},
		  ghosts{},
		  playerVisible(game.phase() != RoundPhase::recovering ||
						std::fmod(game.recoveryTime(), GameConfig::deathBlinkPeriod) < GameConfig::deathBlinkPeriod / 2),
		  powered(game.powerRemaining() > 0),
		  score(game.score()),
		  lives(game.lives()),
		  remaining(game.map().remainingCollectibles()),
		  powerRemaining(game.powerRemaining()),
		  phase(game.phase()),
		  queuedDirection(game.player().queuedDirection()),
		  error(game.error())
	{
		const double renderTime = std::clamp(interpolation, 0.0, 1.0) * GameConfig::fixedStep;
		for (std::size_t i = 0; i < ghosts.size(); ++i) {
			const auto& ghost = game.ghosts()[i];
			Direction direction = Direction::up;
			if (const auto next = ghost.mover.nextNode()) {
				const auto delta = game.graph().edgeDelta(ghost.mover.currentNode(), *next);
				if (delta.x < 0)
					direction = Direction::left;
				else if (delta.x > 0)
					direction = Direction::right;
			}
			auto condition = ghost.condition;
			if (condition == GhostCondition::frightened && game.powerRemaining() < 2 &&
				static_cast<int>(game.powerRemaining() / 0.15) % 2 == 0)
				condition = GhostCondition::normal;
			ghosts[i] = {{game.ghostTraces()[i].positionAt(renderTime), direction}, ghost.kind, condition};
		}
	}

	namespace {
		// 한 고스트의 디버그 표시 자료를 구성한다.
		GhostDebugView inspectGhost(const Game& game, const GhostController& controller, std::size_t i)
		{
			const auto& ghost = game.ghosts()[i];
			const auto& brain = *controller.ghosts()[i];
			const char* state = game.phase() == RoundPhase::recovering			? "Returning"
								: ghost.condition == GhostCondition::eaten		? "Eaten"
								: ghost.condition == GhostCondition::frightened ? "Frightened"
																				: brain.stateName();
			return GhostDebugView{ghost.mover,			 ghost.path, ghost.target,		   ghost.pathSearches, ghost.condition,
								  game.ghostReleased(i), state,		 brain.lastSeenNode(), brain.searching(),  brain.searchWaitRemaining()};
		}
	}	 // namespace

	// 게임과 AI의 디버그 표시 자료를 구성한다.
	DebugView::DebugView(const Game& game, const GhostController& controller)
		: map(game.map()),
		  graph(game.graph()),
		  phase(game.phase()),
		  ghosts{inspectGhost(game, controller, 0), inspectGhost(game, controller, 1), inspectGhost(game, controller, 2)}
	{
	}
}	 // namespace pacman
