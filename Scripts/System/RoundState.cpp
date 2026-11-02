#include "System/RoundState.h"
#include <algorithm>
#include <limits>

namespace pacman {
	namespace {
		constexpr double epsilon = 1e-10;
	}

	// 디버그 조작으로 목숨을 추가한다.
	void RoundState::addDebugLife()
	{
		if (phase == RoundPhase::victory || phase == RoundPhase::error || lives == std::numeric_limits<int>::max())
			return;
		++lives;
		if (phase == RoundPhase::gameOver) {
			phase = RoundPhase::recovering;
			recoveryTime = 0;
		}
	}

	// 하우스에 돌아온 고스트를 부활시킨다.
	void RoundState::reviveGhosts(std::array<GhostActor, 3>& ghosts, NodeId home)
	{
		for (auto& ghost : ghosts) {
			if (ghost.condition == GhostCondition::eaten && ghost.mover.centerNode() == home) {
				ghost.condition = powerRemaining > epsilon ? GhostCondition::frightened : GhostCondition::normal;
				ghost.path.clear();
				ghost.target.reset();
			}
		}
	}

	// 플레이어 사망과 고스트 복귀를 적용한다.
	void RoundState::die(Player& player, std::array<GhostActor, 3>& ghosts)
	{
		--lives;
		powerRemaining = 0;
		player.clearQueuedDirection();
		phase = lives == 0 ? RoundPhase::gameOver : RoundPhase::recovering;
		recoveryTime = 0;
		for (auto& ghost : ghosts) {
			ghost.condition = GhostCondition::normal;
			ghost.path.clear();
			ghost.target.reset();
		}
	}

	// 사망 복귀를 끝내고 게임을 재개한다.
	void RoundState::finishRecovery(std::array<GhostActor, 3>& ghosts)
	{
		phase = RoundPhase::playing;
		activeTime = 0;
		recoveryTime = 0;
		for (auto& ghost : ghosts) {
			ghost.path.clear();
			ghost.target.reset();
		}
	}

	// 수집물의 점수와 효과를 적용한다.
	void RoundState::applyCollectible(std::array<GhostActor, 3>& ghosts, Collectible kind)
	{
		switch (kind) {
			case Collectible::none:
				break;
			case Collectible::pellet:
				score += GameConfig::pelletScore;
				break;
			case Collectible::phaseShuffle:
				score += GameConfig::phaseShuffleScore;
				break;
			case Collectible::powerPellet:
				score += GameConfig::powerPelletScore;
				powerRemaining = GameConfig::powerDuration;
				for (auto& ghost : ghosts)
					if (ghost.condition != GhostCondition::eaten) {
						ghost.condition = GhostCondition::frightened;
						ghost.path.clear();
					}
				break;
		}
	}

	// 라운드 시간과 파워 잔여 시간을 진행한다.
	void RoundState::advancePower(double dt)
	{
		activeTime += dt;
		powerRemaining = std::max(0.0, powerRemaining - dt);
	}

	// 만료된 파워 효과를 해제한다.
	void RoundState::expirePower(std::array<GhostActor, 3>& ghosts)
	{
		if (powerRemaining > epsilon)
			return;
		powerRemaining = 0;
		for (auto& ghost : ghosts)
			if (ghost.condition == GhostCondition::frightened) {
				ghost.condition = GhostCondition::normal;
				ghost.path.clear();
			}
	}

	// 공포 상태 고스트를 포획 처리한다.
	void RoundState::captureGhost(GhostActor& ghost, WorldPosition position, std::vector<GameEvent>& events)
	{
		score += GameConfig::ghostCaptureScore;
		events.push_back({GameEventKind::ghostCaptured, position});
		ghost.condition = GhostCondition::eaten;
		ghost.path.clear();
	}

	// 남은 수집물로 승리 여부를 결정한다.
	void RoundState::finishCollection(int remaining)
	{
		if (lives > 0 && remaining == 0)
			phase = RoundPhase::victory;
	}
}	 // namespace pacman
