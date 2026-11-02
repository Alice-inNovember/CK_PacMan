#pragma once

namespace pacman {

	struct GameConfig
	{
		static constexpr int initialLives = 3;
		static constexpr double fixedStep = 1.0 / 60;
		static constexpr double readyDuration = 2;
		static constexpr double sightPixels = 16 * 32;
		static constexpr double sightAxisTolerance = 4;
		static constexpr double searchWaitSeconds = 1;
		static constexpr double redSpeed = 112;
		static constexpr double ghostSpeed = 128;
		static constexpr double returnSpeed = 192;
		static constexpr double powerDuration = 8;
		static constexpr double frightenedFactor = 0.6;
		static constexpr double frightenedWhiteness = 0.8;
		static constexpr double ghostReleaseInterval = 0.5;
		static constexpr double deathBlinkPeriod = 0.2;
		static constexpr double contactRadius = 20;
		static constexpr int shuffleSafeDistance = 6;
		static constexpr int pelletScore = 100;
		static constexpr int powerPelletScore = 1000;
		static constexpr int ghostCaptureScore = 2000;
		static constexpr int phaseShuffleScore = 500;
	};

}
