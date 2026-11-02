#include "MapEditor/EditorDocument.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace pacman {

	// 새 맵 편집 문서를 만든다.
	EditorDocument::EditorDocument() : mRows(Map::height, std::string(Map::width, '#')), mSaved(mRows) {}

	// 파일을 읽는다.
	void EditorDocument::load(const std::filesystem::path& path)
	{
		std::ifstream input(path, std::ios::binary);
		if (!input)
			throw std::runtime_error("Cannot open: " + path.string());
		Rows candidate;
		std::string row;
		while (std::getline(input, row)) {
			if (!row.empty() && row.back() == '\r')
				row.pop_back();
			if (row.size() != Map::width || row.find_first_not_of("# .osPH") != std::string::npos)
				throw std::runtime_error("Invalid row " + std::to_string(candidate.size() + 1) + ": expected 48 map characters");
			candidate.push_back(row);
			if (candidate.size() > Map::height)
				throw std::runtime_error("Expected exactly 32 rows");
		}
		if (input.bad() || candidate.size() != Map::height)
			throw std::runtime_error("Cannot read 32 map rows");
		mRows = std::move(candidate);
		mSaved = mRows;
		mPath = path;
		mUndo.clear();
		mRedo.clear();
	}

	// 현재 문서를 파일에 저장한다.
	void EditorDocument::save(const std::filesystem::path& path)
	{
		// Write the complete document to a sibling before replacing the destination.
		auto temporary = path;
		temporary += ".MapEditor.tmp";
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		if (!output)
			throw std::runtime_error("Cannot write: " + temporary.string());
		for (const auto& row : mRows)
			output << row << '\n';
		output.close();
		if (!output)
			throw std::runtime_error("Write failed: " + temporary.string());
		std::error_code error;
		std::filesystem::rename(temporary, path, error);
		if (error) {
			std::filesystem::remove(temporary);
			throw std::runtime_error("Cannot replace map: " + error.message());
		}
		mPath = path;
		mSaved = mRows;
	}

	// 새 맵 문서를 만든다.
	void EditorDocument::newMap()
	{
		beginStroke();
		mRows.assign(Map::height, std::string(Map::width, '#'));
		mPath.clear();
	}

	// 그리기 작업의 실행 취소 지점을 만든다.
	void EditorDocument::beginStroke()
	{
		if (mUndo.empty() || mUndo.back() != mRows) {
			if (mUndo.size() == 128)
				mUndo.erase(mUndo.begin());
			mUndo.push_back(mRows);
		}
		mRedo.clear();
	}

	// 기존 시작점 표시를 제거한다.
	void EditorDocument::removeMarker(char marker)
	{
		for (auto& row : mRows)
			for (auto& cell : row)
				if (cell == marker)
					cell = ' ';
	}

	// 선택한 위치에 브러시를 적용한다.
	void EditorDocument::paint(TilePos pos, char brush, TilePos exitDirection)
	{
		if (!Map::contains(pos))
			return;
		if (brush == 'H') {
			const TilePos outside{pos.x + 2 * exitDirection.x, pos.y + 2 * exitDirection.y};
			if (!Map::contains({pos.x - 1, pos.y - 1}) || !Map::contains({pos.x + 1, pos.y + 1}) || !Map::contains(outside))
				return;
			// Remove the old house footprint when moving the unique shared house.
			Rows old = mRows;
			for (int y = 0; y < Map::height; ++y)
				for (int x = 0; x < Map::width; ++x)
					if (old[y][x] == 'H') {
						for (int dy = -1; dy <= 1; ++dy)
							for (int dx = -1; dx <= 1; ++dx)
								if (Map::contains({x + dx, y + dy}))
									mRows[y + dy][x + dx] = ' ';
					}
			for (int dy = -1; dy <= 1; ++dy)
				for (int dx = -1; dx <= 1; ++dx)
					mRows[pos.y + dy][pos.x + dx] = '#';
			mRows[pos.y][pos.x] = 'H';
			mRows[pos.y + exitDirection.y][pos.x + exitDirection.x] = ' ';
			if (mRows[outside.y][outside.x] == '#')
				mRows[outside.y][outside.x] = ' ';
			return;
		}
		if (brush == 'P')
			removeMarker('P');
		mRows[pos.y][pos.x] = brush;
	}

	// 선택 영역에 브러시를 적용한다.
	void EditorDocument::paintRectangle(TilePos from, TilePos to, char brush, TilePos exitDirection)
	{
		beginStroke();
		if (brush == 'P' || brush == 'H') {
			paint(to, brush, exitDirection);
			return;
		}
		const int left = std::clamp(std::min(from.x, to.x), 0, Map::width - 1);
		const int right = std::clamp(std::max(from.x, to.x), 0, Map::width - 1);
		const int top = std::clamp(std::min(from.y, to.y), 0, Map::height - 1);
		const int bottom = std::clamp(std::max(from.y, to.y), 0, Map::height - 1);
		for (int y = top; y <= bottom; ++y)
			for (int x = left; x <= right; ++x)
				paint({x, y}, brush, exitDirection);
	}

	// 마지막 편집을 되돌린다.
	void EditorDocument::undo()
	{
		if (mUndo.empty())
			return;
		mRedo.push_back(mRows);
		mRows = std::move(mUndo.back());
		mUndo.pop_back();
	}

	// 되돌린 편집을 다시 적용한다.
	void EditorDocument::redo()
	{
		if (mRedo.empty())
			return;
		mUndo.push_back(mRows);
		mRows = std::move(mRedo.back());
		mRedo.pop_back();
	}

	// 현재 맵의 유효성을 검사한다.
	std::string EditorDocument::validate() const
	{
		std::ostringstream text;
		for (const auto& row : mRows)
			text << row << '\n';
		std::istringstream input(text.str());
		try {
			Map::parse(input, "map");
			return "VALID";
		} catch (const MapError& error) {
			return error.what();
		}
	}

}	 // namespace pacman
