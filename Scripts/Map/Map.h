#pragma once

#include <array>
#include <filesystem>
#include <istream>
#include <optional>
#include <stdexcept>
#include <string>
#include "Map/MapTypes.h"

namespace pacman {

	class MapError : public std::runtime_error
	{
	   public:
		using std::runtime_error::runtime_error;
	};

	class Map
	{
	   public:
		static constexpr int width = 48;
		static constexpr int height = 32;
		static constexpr int tileSize = 32;
		static Map loadFromFile(const std::filesystem::path& path);
		static Map parse(std::istream& input, const std::string& source = "<stream>");

		static bool contains(TilePos pos);
		static TilePos wrap(TilePos pos);
		std::optional<TilePos> step(TilePos from, TilePos direction) const;
		const Tile& at(TilePos pos) const;
		bool isWalkable(TilePos pos) const;
		bool isPlayerWalkable(TilePos pos) const;

		TilePos playerSpawn() const { return mPlayerSpawn; }

		TilePos house() const { return mHouse; }

		TilePos houseExit() const { return mHouseExit; }

		int count(Collectible kind) const;
		int remainingCollectibles() const;
		int remainingPowerPellets() const { return mRemainingPowerPellets; }
		Collectible collect(TilePos pos);
		WallVisual wallVisual(TilePos pos) const;

	   private:
		Map() = default;
		std::array<Tile, width * height> mTiles{};
		int mRemainingCollectibles = 0;
		int mRemainingPowerPellets = 0;
		TilePos mPlayerSpawn{};
		TilePos mHouse{};
		TilePos mHouseExit{};
	};

}
