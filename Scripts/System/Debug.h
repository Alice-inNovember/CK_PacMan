#pragma once
#include <SFML/Graphics.hpp>
#include "Rendering/GamePresentation.h"

namespace pacman {
	class Debug
	{
	   public:
		void toggle() { mEnabled = !mEnabled; }

		bool enabled() const { return mEnabled; }

		void invalidateMap()
		{
			mCachedGraph = nullptr;
			mSightPositions = {};
		}

		void updateFrameTime(double seconds);
		void drawWorld(sf::RenderTarget&, const sf::Font&, const DebugView&) const;
		void drawHud(sf::RenderTarget&, const sf::Font&, const DebugView&) const;

	   private:
		void cacheGraph(const NavigationGraph&) const;
		void drawDetection(sf::RenderTarget&, const DebugView&) const;
		mutable const NavigationGraph* mCachedGraph = nullptr;
		mutable sf::VertexArray mConnections{sf::Lines}, mOutlines{sf::Lines}, mNodes{sf::Triangles};
		mutable std::array<std::optional<WorldPosition>, 2> mSightPositions;
		mutable std::array<sf::VertexArray, 2> mSightAreas;
		bool mEnabled = false;
		double mFps = 60;
	};
}
