#pragma once
#include <SFML/Graphics.hpp>
#include "Rendering/GamePresentation.h"

namespace pacman {
	class HudRenderer
	{
	   public:
		void draw(sf::RenderTarget&, const sf::Font&, const GamePresentation&, bool paused) const;
		static sf::FloatRect debugLifeButtonBounds();
		void drawDebugLifeButton(sf::RenderTarget&, const sf::Font&, RoundPhase) const;
		void drawOverlay(sf::RenderTarget&, const sf::Font&, const GamePresentation&) const;
	};
}
