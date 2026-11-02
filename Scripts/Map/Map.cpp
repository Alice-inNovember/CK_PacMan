#include "Map/Map.h"

namespace pacman {

	// 좌표가 맵 범위 안에 있는지 확인한다.
	bool Map::contains(TilePos pos)
	{
		return pos.x >= 0 && pos.x < width && pos.y >= 0 && pos.y < height;
	}

	// 타일 좌표를 맵 경계 안으로 보정한다.
	TilePos Map::wrap(TilePos pos)
	{
		return {(pos.x % width + width) % width, (pos.y % height + height) % height};
	}

	// 이동 방향의 인접 타일을 반환한다.
	std::optional<TilePos> Map::step(TilePos from, TilePos direction) const
	{
		if (!isWalkable(from))
			return {};
		const auto to = wrap({from.x + direction.x, from.y + direction.y});
		return isWalkable(to) ? std::optional{to} : std::nullopt;
	}

	// 좌표의 맵 타일을 반환한다.
	const Tile& Map::at(TilePos pos) const
	{
		if (!contains(pos))
			throw std::out_of_range("tile outside map");
		return mTiles[static_cast<std::size_t>(pos.y * width + pos.x)];
	}

	// 타일이 벽이 아닌지 확인한다.
	bool Map::isWalkable(TilePos pos) const
	{
		return contains(pos) && !mTiles[static_cast<std::size_t>(pos.y * width + pos.x)].wall;
	}

	// 플레이어가 타일에 들어갈 수 있는지 확인한다.
	bool Map::isPlayerWalkable(TilePos pos) const
	{
		return isWalkable(pos) && pos != mHouse;
	}

	// 지정한 종류의 수집물을 센다.
	int Map::count(Collectible kind) const
	{
		int total = 0;
		for (const auto& tile : mTiles) {
			if (!tile.wall && tile.collectible == kind)
				++total;
		}
		return total;
	}

	// 남은 수집물 수를 반환한다.
	int Map::remainingCollectibles() const
	{
		return mRemainingCollectibles;
	}

	// 타일의 수집물을 제거하고 종류를 반환한다.
	Collectible Map::collect(TilePos pos)
	{
		at(pos);
		auto& tile = mTiles[static_cast<std::size_t>(pos.y * width + pos.x)];
		const auto result = tile.collectible;
		if (result != Collectible::none)
			--mRemainingCollectibles;
		if (result == Collectible::powerPellet)
			--mRemainingPowerPellets;
		tile.collectible = Collectible::none;
		return result;
	}

}	 // namespace pacman
