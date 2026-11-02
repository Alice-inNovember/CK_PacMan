#pragma once
#include "AI/Ghost.h"

namespace pacman {
	class Game;

	class GhostController
	{
	   public:
		GhostController();
		void reset();
		GhostTargets advance(const Game&, double dt);
		GhostTarget planAtNode(const AiContext&);

		const GhostTeam& ghosts() const { return mGhosts; }

	   private:
		GhostTeam mGhosts;
	};
}
