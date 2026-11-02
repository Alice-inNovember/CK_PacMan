#pragma once
#include "Player/Player.h"
#include "System/GhostActor.h"
namespace pacman {
	enum class RoundPhase
	{
		ready,
		playing,
		recovering,
		victory,
		gameOver,
		error
	};
	enum class GameEventKind
	{
		ghostCaptured
	};

	struct GameEvent
	{
		GameEventKind kind;
		WorldPosition position;
	};

	struct RoundState
	{
		void addDebugLife();
		void reviveGhosts(std::array<GhostActor, 3>& ghosts, NodeId home);
		void die(Player& player, std::array<GhostActor, 3>& ghosts);
		void finishRecovery(std::array<GhostActor, 3>& ghosts);
		void applyCollectible(std::array<GhostActor, 3>& ghosts, Collectible kind);
		void advancePower(double dt);
		void expirePower(std::array<GhostActor, 3>& ghosts);
		void captureGhost(GhostActor& ghost, WorldPosition position, std::vector<GameEvent>& events);
		void finishCollection(int remaining);

		RoundPhase phase = RoundPhase::ready;
		int score = 0;
		int lives = GameConfig::initialLives;
		double readyRemaining = GameConfig::readyDuration;
		double activeTime = 0;
		double recoveryTime = 0;
		double powerRemaining = 0;
		std::string error;
	};

}
