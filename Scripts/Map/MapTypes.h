#pragma once

#include <cstdint>

namespace pacman {

	struct TilePos
	{
		int x;
		int y;
		bool operator==(const TilePos&) const = default;
	};

	enum class Collectible
	{
		none,
		pellet,
		powerPellet,
		phaseShuffle
	};
	enum class GhostKind
	{
		red,
		green,
		blue
	};
	enum class WallShape
	{
		isolated,
		end,
		straight,
		corner,
		tee,
		cross
	};

	struct Tile
	{
		bool wall = false;
		Collectible collectible = Collectible::none;
	};

	struct WallVisual
	{
		WallShape shape;
		int rotationDegrees;
		std::uint8_t mask;
	};

	WallVisual makeWallVisual(std::uint8_t mask);

}
