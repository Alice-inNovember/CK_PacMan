#pragma once

#include <filesystem>
#include <vector>
#include "Map/Map.h"

namespace pacman {

	class EditorDocument
	{
	   public:
		using Rows = std::vector<std::string>;
		EditorDocument();
		void load(const std::filesystem::path& path);
		void save(const std::filesystem::path& path);
		void newMap();
		void beginStroke();
		void paint(TilePos position, char brush, TilePos exitDirection);
		void paintRectangle(TilePos from, TilePos to, char brush, TilePos exitDirection);
		void undo();
		void redo();

		bool dirty() const { return mRows != mSaved; }

		char at(TilePos pos) const { return mRows.at(pos.y).at(pos.x); }

		std::string validate() const;

		const std::filesystem::path& path() const { return mPath; }

	   private:
		void removeMarker(char marker);
		Rows mRows;
		Rows mSaved;
		std::filesystem::path mPath;
		std::vector<Rows> mUndo;
		std::vector<Rows> mRedo;
	};
}
