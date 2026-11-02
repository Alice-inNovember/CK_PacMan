#pragma once

#include <SFML/Graphics/Rect.hpp>

namespace pacman {

	class SpriteAnimator
	{
	   public:
		SpriteAnimator(int frameWidth, int frameHeight, int frameCount, double frameSeconds, bool loop);
		void update(double dt);
		sf::IntRect frame() const;
		bool finished() const;

	   private:
		int mWidth;
		int mHeight;
		int mCount;
		double mFrameSeconds;
		bool mLoop;
		double mElapsed = 0;
	};
}
