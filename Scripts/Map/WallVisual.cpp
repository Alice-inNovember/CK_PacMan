#include "Map/Map.h"

namespace pacman {
	namespace {
		constexpr std::array<TilePos, 4> directions{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
	}

	// 타일의 벽 이미지 형태와 회전을 구한다.
	WallVisual Map::wallVisual(TilePos pos) const
	{
		if (!at(pos).wall)
			throw std::invalid_argument("wallVisual requires a wall tile");
		unsigned mask = 0;
		for (std::size_t i = 0; i < directions.size(); ++i) {
			TilePos next{pos.x + directions[i].x, pos.y + directions[i].y};
			if (contains(next) && at(next).wall)
				mask |= 1U << i;
		}
		return makeWallVisual(static_cast<std::uint8_t>(mask));
	}

	// 벽 연결 마스크를 이미지 형태와 회전으로 변환한다.
	WallVisual makeWallVisual(std::uint8_t mask)
	{
		if (mask > 15)
			throw std::invalid_argument("wall mask must be between 0 and 15");
		// Canonical art: end up, straight up/down, L up/right, T up/right/left.
		static constexpr std::array<WallVisual, 16> visuals{{{WallShape::isolated, 0, 0},
															 {WallShape::end, 0, 1},
															 {WallShape::end, 90, 2},
															 {WallShape::corner, 0, 3},
															 {WallShape::end, 180, 4},
															 {WallShape::straight, 0, 5},
															 {WallShape::corner, 90, 6},
															 {WallShape::tee, 90, 7},
															 {WallShape::end, 270, 8},
															 {WallShape::corner, 270, 9},
															 {WallShape::straight, 90, 10},
															 {WallShape::tee, 0, 11},
															 {WallShape::corner, 180, 12},
															 {WallShape::tee, 270, 13},
															 {WallShape::tee, 180, 14},
															 {WallShape::cross, 0, 15}}};
		return visuals[mask];
	}

}	 // namespace pacman
