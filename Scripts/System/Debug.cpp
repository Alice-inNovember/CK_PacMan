#include "System/Debug.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include "AI/Ghost.h"
#include "Rendering/UiStyle.h"

namespace pacman {
	namespace {
		// 디버그 표시용 텍스트를 구성한다.
		void label(sf::RenderTarget& target, const sf::Font& font, const std::string& value, float x, float y, unsigned size,
				   sf::Color color = sf::Color::White)
		{
			sf::Text text(value, font, size);
			text.setPosition(x, y);
			text.setFillColor(color);
			target.draw(text);
		}

		// 굵기가 있는 선의 정점을 추가한다.
		void appendThickLine(sf::VertexArray& lines, sf::Vector2f start, sf::Vector2f end, sf::Vector2f normal, sf::Color color,
							 int thickness)
		{
			for (int offset = -thickness; offset <= thickness; ++offset) {
				lines.append({start + normal * static_cast<float>(offset), color});
				lines.append({end + normal * static_cast<float>(offset), color});
			}
		}

		// 탐색 노드의 표시 정점을 추가한다.
		void appendNode(sf::VertexArray& nodes, sf::Vector2f center, sf::Color color)
		{
			constexpr int sides = 20;
			constexpr double pi = 3.14159265358979323846;
			for (int i = 0; i < sides; ++i) {
				const double a = 2 * pi * i / sides, b = 2 * pi * (i + 1) / sides;
				const sf::Vector2f p{static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a))};
				const sf::Vector2f q{static_cast<float>(std::cos(b)), static_cast<float>(std::sin(b))};
				for (const auto vertex : {center, center + p * 6.5f, center + q * 6.5f})
					nodes.append({vertex, {12, 20, 30}});
				for (const auto vertex : {center, center + p * 5.f, center + q * 5.f})
					nodes.append({vertex, color});
			}
		}

		// 디버그 텍스트 한 줄을 구성한다.
		void debugLine(sf::VertexArray& lines, WorldPosition from, WorldPosition to, sf::Color color, bool wrap, int thickness)
		{
			const auto delta = wrap ? wrappedDelta(from, to) : WorldPosition{to.x - from.x, to.y - from.y};
			const sf::Vector2f normal = std::abs(delta.x) < 0.01 ? sf::Vector2f{1, 0} : sf::Vector2f{0, 1};
			for (int x = wrap ? -1 : 0; x <= (wrap ? 1 : 0); ++x)
				for (int y = wrap ? -1 : 0; y <= (wrap ? 1 : 0); ++y) {
					const sf::Vector2f start{static_cast<float>(from.x + x * Map::width * Map::tileSize),
											 static_cast<float>(from.y + y * Map::height * Map::tileSize)};
					const sf::Vector2f end = start + sf::Vector2f{static_cast<float>(delta.x), static_cast<float>(delta.y)};
					appendThickLine(lines, start, end, normal, color, thickness);
				}
		}

		// 사각형의 정점을 추가한다.
		void appendQuad(sf::VertexArray& areas, sf::Vector2f at, sf::Vector2f size, sf::Color color)
		{
			for (auto vertex : {at, at + sf::Vector2f{size.x, 0}, at + size, at + sf::Vector2f{0, size.y}})
				areas.append({vertex, color});
		}

		// 경계 순환을 고려해 사각형을 추가한다.
		void appendWrappedQuad(sf::VertexArray& areas, sf::Vector2f origin, sf::Vector2f size, sf::Color color)
		{
			for (int x = -1; x <= 1; ++x)
				for (int y = -1; y <= 1; ++y)
					appendQuad(areas,
							   origin + sf::Vector2f{static_cast<float>(x * Map::width * Map::tileSize),
													 static_cast<float>(y * Map::height * Map::tileSize)},
							   size, color);
		}

		// 연속된 시야 구간을 추가한다.
		void appendSightRun(sf::VertexArray& areas, WorldPosition position, sf::Color color, bool horizontal, int offset, int sign,
							int first, int low)
		{
			constexpr int stepPixels = 2;
			const float center = sign * (first + low) * stepPixels / 2.f;
			const float length = (low - first + 1) * stepPixels;
			const sf::Vector2f size = horizontal ? sf::Vector2f{length, 2} : sf::Vector2f{2, length};
			const sf::Vector2f origin{static_cast<float>(position.x + (horizontal ? center : offset)) - size.x / 2,
									  static_cast<float>(position.y + (horizontal ? offset : center)) - size.y / 2};
			appendWrappedQuad(areas, origin, size, color);
		}

		// 한 방향의 시야 구간을 검사한다.
		void appendSightRay(sf::VertexArray& areas, const Map& map, WorldPosition position, sf::Color color, bool horizontal, int offset,
							int sign)
		{
			constexpr int stepPixels = 2;
			const int maxStep = static_cast<int>(GameConfig::sightPixels) / stepPixels;
			const int tolerance = static_cast<int>(GameConfig::sightAxisTolerance);
			const auto visible = [&](int step) {
				const double along = sign * step * stepPixels;
				const auto sample = wrapPosition({position.x + (horizontal ? along : offset), position.y + (horizontal ? offset : along)});
				const TilePos tile{static_cast<int>(sample.x / Map::tileSize), static_cast<int>(sample.y / Map::tileSize)};
				return map.isWalkable(tile) && Ghost::canSeePosition(map, position, sample);
			};
			const int begin = horizontal ? (sign > 0 ? 0 : 1) : tolerance / stepPixels + 1;
			std::optional<int> start;
			for (int step = begin; step <= maxStep + 1; ++step) {
				const bool seen = step <= maxStep && visible(step);
				if (seen && !start)
					start = step;
				if (!seen && start) {
					appendSightRun(areas, position, color, horizontal, offset, sign, *start, step - 1);
					start.reset();
				}
			}
		}

		// 시야 띠의 표시 정점을 구성한다.
		void appendSightStrip(sf::VertexArray& areas, const Map& map, WorldPosition position, sf::Color color, bool horizontal)
		{
			const int tolerance = static_cast<int>(GameConfig::sightAxisTolerance);
			for (int offset = -tolerance; offset <= tolerance; offset += 2)
				for (int sign : {-1, 1})
					appendSightRay(areas, map, position, color, horizontal, offset, sign);
		}

		// 고스트의 시야 표시 영역을 만든다.
		void buildSightArea(sf::VertexArray& areas, const Map& map, WorldPosition position, sf::Color color)
		{
			for (bool horizontal : {true, false})
				appendSightStrip(areas, map, position, color, horizontal);
		}
	}	 // namespace

	// 맵의 탐색 그래프 표시를 캐시한다.
	void Debug::cacheGraph(const NavigationGraph& graph) const
	{
		if (mCachedGraph == &graph)
			return;
		mCachedGraph = &graph;
		mSightPositions = {};
		mConnections.clear();
		mOutlines.clear();
		mNodes.clear();
		for (std::size_t n = 0; n < graph.nodes().size(); ++n) {
			const auto& from = graph.at(NodeId{n});
			for (auto neighbor : from.neighbors) {
				if (neighbor.value <= n)
					continue;
				const auto& to = graph.at(neighbor);
				const bool wrap = graph.isWrapEdge(NodeId{n}, neighbor);
				debugLine(mOutlines, {from.centerX, from.centerY}, {to.centerX, to.centerY}, {8, 35, 45, 210}, wrap, 1);
				debugLine(mConnections, {from.centerX, from.centerY}, {to.centerX, to.centerY}, {45, 220, 245, 210}, wrap, 0);
			}
		}

		for (std::size_t n = 0; n < graph.nodes().size(); ++n) {
			const auto& node = graph.at(NodeId{n});
			const auto color = graph.isJunction(NodeId{n}) ? sf::Color(255, 190, 60) : sf::Color(70, 210, 255);
			appendNode(mNodes, {node.centerX, node.centerY}, color);
		}
	}

	// 고스트의 시야를 표시한다.
	void Debug::drawDetection(sf::RenderTarget& target, const DebugView& view) const
	{
		if (view.phase != RoundPhase::playing)
			return;
		for (std::size_t index = 1; index < 3; ++index) {
			if (view.ghosts[index].condition != GhostCondition::normal || !view.ghosts[index].released)
				continue;
			const auto position = view.ghosts[index].mover.position(view.graph);
			const auto cacheIndex = index - 1;
			auto& areas = mSightAreas[cacheIndex];
			const auto previous = mSightPositions[cacheIndex];
			if (previous && previous->x == position.x && previous->y == position.y) {
				target.draw(areas);
				continue;
			}
			mSightPositions[cacheIndex] = position;
			areas = sf::VertexArray(sf::Quads);
			const auto color = index == 1 ? sf::Color(60, 255, 100, 160) : sf::Color(80, 150, 255, 160);
			buildSightArea(areas, view.map, position, color);
			target.draw(areas);
		}
	}

	// 월드 좌표의 디버그 정보를 그린다.
	void Debug::drawWorld(sf::RenderTarget& target, const sf::Font& font, const DebugView& view) const
	{
		if (!mEnabled)
			return;
		cacheGraph(view.graph);
		drawDetection(target, view);
		target.draw(mOutlines);
		target.draw(mConnections);
		const auto& ghost = view.ghosts[0];
		if (!ghost.path.empty()) {
			sf::VertexArray lines(sf::Lines);
			auto previous = ghost.mover.position(view.graph);
			auto previousNode = ghost.mover.currentNode();
			auto start = std::find(ghost.path.begin(), ghost.path.end(), ghost.mover.planningNode());
			for (auto it = start; it != ghost.path.end(); ++it) {
				const auto& next = view.graph.at(*it);
				debugLine(lines, previous, {next.centerX, next.centerY}, {255, 65, 80}, view.graph.isWrapEdge(previousNode, *it), 1);
				previousNode = *it;
				previous = {next.centerX, next.centerY};
			}
			target.draw(lines);
		}
		target.draw(mNodes);
		for (std::size_t i = 0; i < 3; ++i) {
			const auto& actor = view.ghosts[i];
			if (view.phase == RoundPhase::playing && actor.target && std::string(view.ghosts[i].stateName) == "Patrol") {
				constexpr std::array<const char*, 3> names{"Red", "Green", "Blue"};
				const std::array<sf::Color, 3> colors{sf::Color(255, 80, 90), sf::Color(100, 255, 120), sf::Color(110, 160, 255)};
				const auto& node = view.graph.at(*actor.target);
				sf::CircleShape ring(12 + static_cast<float>(i) * 3);
				ring.setOrigin(ring.getRadius(), ring.getRadius());
				ring.setPosition(node.centerX, node.centerY);
				ring.setFillColor(sf::Color::Transparent);
				ring.setOutlineColor(colors[i]);
				ring.setOutlineThickness(3);
				target.draw(ring);
				sf::Text caption(std::string(names[i]) + " Patrol target #" + std::to_string(actor.target->value), font, ui::small);
				caption.setPosition(std::clamp(node.centerX + 20, 0.f, 1300.f),
									std::max(0.f, node.centerY - 42 + static_cast<float>(i) * 20));
				caption.setFillColor(colors[i]);
				caption.setOutlineColor(sf::Color::Black);
				caption.setOutlineThickness(1.5f);
				target.draw(caption);
			}
			if (i > 0 && view.phase == RoundPhase::playing && actor.condition == GhostCondition::normal &&
				std::string(view.ghosts[i].stateName) == "Chase" && view.ghosts[i].lastSeenNode) {
				const auto& seen = view.graph.at(*view.ghosts[i].lastSeenNode);
				const sf::Color color = i == 1 ? sf::Color(100, 255, 120) : sf::Color(110, 160, 255);
				sf::CircleShape marker(9, 4);
				marker.setOrigin(9, 9);
				marker.setPosition(seen.centerX, seen.centerY);
				marker.setFillColor(sf::Color::Transparent);
				marker.setOutlineThickness(2);
				marker.setOutlineColor(color);
				target.draw(marker);
				std::ostringstream caption;
				caption << (i == 1 ? "Green" : "Blue") << " last seen #" << view.ghosts[i].lastSeenNode->value;
				if (view.ghosts[i].searching)
					caption << " search " << std::fixed << std::setprecision(1) << view.ghosts[i].searchWaitRemaining << "s";
				sf::Text text(caption.str(), font, ui::small);
				text.setPosition(std::clamp(seen.centerX + 14, 0.f, 1250.f), std::max(0.f, seen.centerY - 24));
				text.setFillColor(color);
				text.setOutlineColor(sf::Color::Black);
				text.setOutlineThickness(1.5f);
				target.draw(text);
			}
			const auto pos = view.ghosts[i].mover.position(view.graph);
			label(target, font, std::string(view.ghosts[i].stateName), static_cast<float>(pos.x) + 10, static_cast<float>(pos.y) - 25,
				  ui::small);
		}
	}

	// 프레임 시간 통계를 갱신한다.
	void Debug::updateFrameTime(double seconds)
	{
		if (seconds > 0)
			mFps = mFps * 0.95 + 0.05 / seconds;
	}

	// HUD에 디버그 정보를 그린다.
	void Debug::drawHud(sf::RenderTarget& target, const sf::Font& font, const DebugView& view) const
	{
		if (!mEnabled)
			return;

		label(target, font, "AI DEBUG", ui::left, 542, ui::medium, {243, 218, 130});
		constexpr std::array<const char*, 3> names{"Red", "Green", "Blue"};
		for (std::size_t i = 0; i < 3; ++i)
			label(target, font, std::string(names[i]) + ": " + view.ghosts[i].stateName, ui::left, 578 + ui::row * i, ui::small);
		label(target, font, "FPS: " + std::to_string(static_cast<int>(std::round(mFps))), ui::left, 662, ui::small);
		label(target, font, "Red path: " + std::to_string(view.ghosts[0].path.size()) + " nodes", ui::left, 686, ui::small);
		label(target, font, "Red A*: " + std::to_string(view.ghosts[0].pathSearches) + " searches", ui::left, 710, ui::small);
		label(target, font, "Graph: " + std::to_string(view.graph.nodes().size()) + " nodes", ui::left, 734, ui::small);
		label(target, font, "SIGHT", ui::left, 770, ui::small, {150, 170, 183});
		label(target, font, "Red: unlimited chase", ui::left, 794, ui::small);
		label(target, font, "Green/Blue: 6 tiles, walls block", ui::left, 818, ui::small);
		label(target, font, "Search wait: 1s", ui::left, 842, ui::small);
		label(target, font, "MAP LEGEND", ui::left, 878, ui::small, {150, 170, 183});
		label(target, font, "Cyan: connection", ui::left, 902, ui::small, {45, 220, 245});
		label(target, font, "Orange: junction / Blue: node", ui::left, 926, ui::small);
		label(target, font, "Red: active A* path", ui::left, 950, ui::small, {255, 100, 110});
		label(target, font, "Colored ring: Patrol target", ui::left, 974, ui::small);
	}
}	 // namespace pacman
