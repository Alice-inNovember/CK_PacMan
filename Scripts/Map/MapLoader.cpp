#include "Map/Map.h"

#include <fstream>
#include <queue>
#include <vector>

namespace pacman {
	namespace {
		// 타일 좌표의 배열 인덱스를 구한다.
		std::size_t index(TilePos pos)
		{
			return static_cast<std::size_t>(pos.y * Map::width + pos.x);
		}

		constexpr std::array<TilePos, 4> directions{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

		[[noreturn]] void fail(const std::string& source, TilePos pos, const std::string& message)
		{
			throw MapError(source + ":" + std::to_string(pos.y + 1) + ":" + std::to_string(pos.x + 1) + ": " + message);
		}
	}	 // namespace

	// 맵 파일을 읽고 유효성을 검사한다.
	Map Map::loadFromFile(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
			throw MapError(path.string() + ": cannot open map");
		return parse(file, path.string());
	}

	// 입력 스트림에서 맵을 읽고 검증한다.
	Map Map::parse(std::istream& input, const std::string& source)
	{
		Map map;
		std::vector<std::string> rows;
		std::string row;
		while (std::getline(input, row)) {
			if (!row.empty() && row.back() == '\r')
				row.pop_back();
			rows.push_back(row);
			if (rows.size() > height)
				fail(source, {0, height}, "expected 32 rows");
		}
		if (input.bad())
			throw MapError(source + ": read failure");
		if (rows.size() != height)
			throw MapError(source + ": expected 32 rows, got " + std::to_string(rows.size()));

		std::array<int, 2> spawnCounts{};
		for (int y = 0; y < height; ++y) {
			if (rows[y].size() != width)
				fail(source, {0, y}, "expected 48 characters, got " + std::to_string(rows[y].size()));
			for (int x = 0; x < width; ++x) {
				const TilePos pos{x, y};
				Tile& tile = map.mTiles[index(pos)];
				switch (rows[y][x]) {
					case '#':
						tile.wall = true;
						break;
					case ' ':
					case '.':
						tile.collectible = Collectible::pellet;
						break;
					case 'o':
						tile.collectible = Collectible::powerPellet;
						break;
					case 's':
						tile.collectible = Collectible::phaseShuffle;
						break;
					case 'P':
						++spawnCounts[0];
						map.mPlayerSpawn = pos;
						break;
					case 'H':
						++spawnCounts[1];
						map.mHouse = pos;
						break;
					default:
						fail(source, pos, "unknown tile byte " + std::to_string(static_cast<unsigned char>(rows[y][x])));
				}
			}
		}
		constexpr std::array<char, 2> spawnSymbols{'P', 'H'};
		for (std::size_t i = 0; i < spawnCounts.size(); ++i) {
			if (spawnCounts[i] != 1)
				throw MapError(source + ": expected exactly one " + spawnSymbols[i] + ", got " + std::to_string(spawnCounts[i]));
		}

		const auto center = map.mHouse;
		if (!contains({center.x - 1, center.y - 1}) || !contains({center.x + 1, center.y + 1}))
			fail(source, center, "house requires a complete 3x3 footprint");
		int exits = 0;
		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				if (dx == 0 && dy == 0)
					continue;
				const TilePos pos{center.x + dx, center.y + dy};
				if (dx != 0 && dy != 0) {
					if (!map.at(pos).wall)
						fail(source, pos, "house corners must be walls");
				} else if (!map.at(pos).wall) {
					if (rows[pos.y][pos.x] != ' ')
						fail(source, pos, "house exit must be an empty path (space)");
					++exits;
					map.mHouseExit = pos;
				}
			}
		}
		if (exits != 1)
			fail(source, center, "house requires exactly one orthogonal exit, got " + std::to_string(exits));
		const TilePos outside{2 * map.mHouseExit.x - center.x, 2 * map.mHouseExit.y - center.y};
		if (!map.isWalkable(outside))
			fail(source, map.mHouseExit, "house exit must connect outward to a walkable tile");

		map.mTiles[index(map.mHouseExit)].collectible = Collectible::none;
		map.mRemainingPowerPellets = map.count(Collectible::powerPellet);
		map.mRemainingCollectibles = map.count(Collectible::pellet) + map.mRemainingPowerPellets + map.count(Collectible::phaseShuffle);
		if (map.remainingCollectibles() == 0)
			throw MapError(source + ": map has no collectibles");

		std::array<bool, width * height> visited{};
		std::queue<TilePos> frontier;
		visited[index(map.mPlayerSpawn)] = true;
		frontier.push(map.mPlayerSpawn);
		while (!frontier.empty()) {
			const auto current = frontier.front();
			frontier.pop();
			for (auto delta : directions) {
				const auto destination = map.step(current, delta);
				if (!destination)
					continue;
				const auto next = *destination;
				if (map.isWalkable(next) && !visited[index(next)]) {
					visited[index(next)] = true;
					frontier.push(next);
				}
			}
		}
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				TilePos pos{x, y};
				if (map.isWalkable(pos) && !visited[index(pos)])
					fail(source, pos, "walkable tile disconnected from P");
			}
		}
		visited.fill(false);
		visited[index(map.mPlayerSpawn)] = true;
		frontier.push(map.mPlayerSpawn);
		while (!frontier.empty()) {
			const auto current = frontier.front();
			frontier.pop();
			for (auto delta : directions) {
				const auto destination = map.step(current, delta);
				if (!destination)
					continue;
				const auto next = *destination;
				if (map.isPlayerWalkable(next) && !visited[index(next)]) {
					visited[index(next)] = true;
					frontier.push(next);
				}
			}
		}
		for (int y = 0; y < height; ++y)
			for (int x = 0; x < width; ++x)
				if (map.at({x, y}).collectible != Collectible::none && !visited[index({x, y})])
					fail(source, {x, y}, "collectible unreachable by player");
		return map;
	}

}	 // namespace pacman
