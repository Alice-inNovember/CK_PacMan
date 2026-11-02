#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <string_view>
#include "MapEditor/EditorDocument.h"
#include "Rendering/TextureStore.h"

namespace {
	using namespace pacman;
	constexpr float editorTileSize = 24;
	constexpr float gridX = 24;
	constexpr float gridY = 88;
	constexpr float sidebar = 1200;
	constexpr std::array<char, 7> brushes{'#', ' ', 'o', 's', 'P', 'H', ' '};
	constexpr std::array<const char*, 7> brushNames{
		"1  Wall", "2  Floor", "3  Power / Fruit", "4  Phase / Lotus", "5  Player start", "6  House (R: rotate)", "7  Eraser / floor"};
	constexpr std::array<TilePos, 4> directions{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

	// SFML 문자열을 UTF-8로 변환한다.
	std::string utf8(const sf::String& text)
	{
		const auto bytes = text.toUtf8();
		return {bytes.begin(), bytes.end()};
	}

	// UTF-8 문자열을 파일 경로로 변환한다.
	std::filesystem::path pathFromUtf8(std::string_view value)
	{
		return std::filesystem::path(std::u8string(value.begin(), value.end()));
	}

	class Editor
	{
	   public:
		// 에디터 창과 문서, 에셋을 초기화한다.
		Editor(const std::filesystem::path& executable, const std::filesystem::path& map, const std::filesystem::path& fontPath)
			: mWindow(sf::VideoMode(1280, 800), "Map Editor"), mTextures(executable.parent_path().parent_path() / "Assets")
		{
			if (!mFont.loadFromFile(fontPath.string()))
				throw std::runtime_error("Cannot load editor font: " + fontPath.string());
			mWindow.setFramerateLimit(60);
			mWindow.setKeyRepeatEnabled(true);
			if (std::filesystem::exists(map))
				mDocument.load(map);
			else
				mStatus = "New map. Choose Save to set the destination.";
			mValidation = mDocument.validate();
		}

		// 창이 열려 있는 동안 이벤트 처리와 화면 갱신을 반복한다.
		void run()
		{
			while (mWindow.isOpen()) {
				sf::Event event{};
				while (mWindow.pollEvent(event))
					handle(event);
				if (!mWindow.isOpen())
					break;
				render();
			}
		}

	   private:
		enum class Modal
		{
			none,
			open,
			save,
			unsaved
		};
		enum class Action
		{
			none,
			quit,
			open,
			newMap
		};
		sf::RenderWindow mWindow;
		sf::Font mFont;
		TextureStore mTextures;
		EditorDocument mDocument;
		int mSelected = 0;
		int mExitDirection = 0;
		bool mGrid = true;
		bool mStroke = false;
		bool mErasing = false;
		bool mRectangleMode = true;
		bool mRectangleStroke = false;
		std::optional<TilePos> mRangeStart;
		std::optional<TilePos> mRangeEnd;
		std::optional<TilePos> mLast;
		Modal mModal = Modal::none;
		Action mPending = Action::none;
		sf::String mInput;
		bool mReplaceInput = false;
		std::string mStatus = "Drag to paint. Right-drag erases. House stamps a 3x3 footprint.";
		std::string mValidation;

		// 에디터 화면에 문자열을 그린다.
		void text(const std::string& value, float x, float y, unsigned size = 18, sf::Color color = sf::Color(225, 230, 236))
		{
			sf::Text label(sf::String::fromUtf8(value.begin(), value.end()), mFont, size);
			label.setFillColor(color);
			label.setPosition(x, y);
			mWindow.draw(label);
		}

		// 배경과 테두리가 있는 패널을 그린다.
		void panel(sf::FloatRect rect, sf::Color fill, sf::Color outline = sf::Color::Transparent)
		{
			sf::RectangleShape shape({rect.width, rect.height});
			shape.setPosition(rect.left, rect.top);
			shape.setFillColor(fill);
			shape.setOutlineColor(outline);
			shape.setOutlineThickness(1);
			mWindow.draw(shape);
		}

		// 에디터 버튼을 그린다.
		void button(sf::FloatRect rect, const std::string& label, bool active = false)
		{
			panel(rect, active ? sf::Color(47, 100, 91) : sf::Color(43, 50, 61), active ? sf::Color(115, 225, 174) : sf::Color(69, 80, 94));
			text(label, rect.left + 14, rect.top + 10);
		}

		// 에셋의 지정 영역을 스프라이트로 그린다.
		void sprite(TextureId texture, sf::IntRect rect, float x, float y, float scale, float angle = 0)
		{
			sf::Sprite image(mTextures.at(texture), rect);
			image.setOrigin(rect.width / 2.f, rect.height / 2.f);
			image.setPosition(x, y);
			image.setScale(scale, scale);
			image.setRotation(angle);
			mWindow.draw(image);
		}

		// 화면 좌표에 해당하는 맵 타일을 찾는다.
		std::optional<TilePos> tileAt(sf::Vector2f mouse) const
		{
			if (mouse.x < gridX || mouse.y < gridY || mouse.x >= gridX + Map::width * editorTileSize ||
				mouse.y >= gridY + Map::height * editorTileSize)
				return {};
			return TilePos{static_cast<int>((mouse.x - gridX) / editorTileSize), static_cast<int>((mouse.y - gridY) / editorTileSize)};
		}

		// 현재 드래그 범위를 갱신한다.
		void updateRange(sf::Vector2f point)
		{
			mRangeEnd = TilePos{std::clamp(static_cast<int>(std::floor((point.x - gridX) / editorTileSize)), 0, Map::width - 1),
								std::clamp(static_cast<int>(std::floor((point.y - gridY) / editorTileSize)), 0, Map::height - 1)};
		}

		// 선택한 타일에 현재 브러시를 적용한다.
		void apply(TilePos pos)
		{
			const char brush = mErasing ? ' ' : brushes[mSelected];
			if (!mLast || brush == 'H')
				mDocument.paint(pos, brush, directions[mExitDirection]);
			else {
				int x = mLast->x, y = mLast->y;
				const int dx = std::abs(pos.x - x), dy = -std::abs(pos.y - y);
				const int sx = x < pos.x ? 1 : -1, sy = y < pos.y ? 1 : -1;
				int error = dx + dy;
				while (true) {
					mDocument.paint({x, y}, brush, directions[mExitDirection]);
					if (x == pos.x && y == pos.y)
						break;
					const int twice = 2 * error;
					if (twice >= dy) {
						error += dy;
						x += sx;
					}
					if (twice <= dx) {
						error += dx;
						y += sy;
					}
				}
			}
			mLast = pos;
		}

		// 진행 중인 드래그 편집을 끝낸다.
		void stopStroke(bool commit = false)
		{
			if (mRectangleStroke && commit && mRangeStart && mRangeEnd)
				mDocument.paintRectangle(*mRangeStart, *mRangeEnd, mErasing ? ' ' : brushes[mSelected], directions[mExitDirection]);
			mRectangleStroke = false;
			mRangeStart.reset();
			mRangeEnd.reset();
			if (mStroke)
				mValidation = mDocument.validate();
			mStroke = false;
			mLast.reset();
		}

		// 파일 경로 입력 창을 연다.
		void pathModal(Modal modal)
		{
			stopStroke();
			mModal = modal;
			const auto path = mDocument.path().empty() ? std::filesystem::path("Maps/NewLevel.txt") : mDocument.path();
			const auto value = std::filesystem::absolute(path).string();
			mInput = sf::String::fromUtf8(value.begin(), value.end());
			mReplaceInput = true;
		}

		// 요청된 파일 작업을 실행한다.
		void perform(Action action)
		{
			mPending = Action::none;
			if (action == Action::quit)
				mWindow.close();
			else if (action == Action::open)
				pathModal(Modal::open);
			else if (action == Action::newMap) {
				mDocument.newMap();
				mValidation = mDocument.validate();
				mModal = Modal::none;
				mStatus = "New wall-filled map. Paint floor, one player and one house.";
			}
		}

		// 저장되지 않은 변경을 확인하고 파일 작업을 요청한다.
		void request(Action action)
		{
			stopStroke();
			if (mDocument.dirty()) {
				mPending = action;
				mModal = Modal::unsaved;
			} else
				perform(action);
		}

		// 입력한 경로로 파일 작업을 수행한다.
		void submitPath()
		{
			try {
				const auto path = pathFromUtf8(utf8(mInput));
				if (path.empty())
					throw std::runtime_error("Enter a file path");
				if (mModal == Modal::open) {
					mDocument.load(path);
					mValidation = mDocument.validate();
					mStatus = "Opened: " + path.string();
					mModal = Modal::none;
				} else {
					mDocument.save(path);
					mValidation = mDocument.validate();
					mStatus =
						mValidation == "VALID" ? "Saved valid map: " + path.string() : "Saved draft (not valid yet): " + path.string();
					mModal = Modal::none;
					const auto action = mPending;
					mPending = Action::none;
					if (action != Action::none)
						perform(action);
				}
			} catch (const std::exception& error) {
				mStatus = error.what();
			}
		}

		// 모달 창의 클릭을 처리한다.
		void clickModal(sf::Vector2f pos)
		{
			if (mModal == Modal::unsaved) {
				if (sf::FloatRect(430, 500, 200, 48).contains(pos))
					pathModal(Modal::save);
				else if (sf::FloatRect(650, 500, 200, 48).contains(pos)) {
					const auto action = mPending;
					mModal = Modal::none;
					perform(action);
				} else if (sf::FloatRect(870, 500, 200, 48).contains(pos)) {
					mModal = Modal::none;
					mPending = Action::none;
				}
			} else {
				if (sf::FloatRect(920, 500, 150, 48).contains(pos))
					submitPath();
				else if (sf::FloatRect(750, 500, 150, 48).contains(pos)) {
					mModal = Modal::none;
					mPending = Action::none;
				} else if (sf::FloatRect(400, 420, 700, 50).contains(pos))
					mReplaceInput = true;
			}
		}

		// 에디터의 창과 입력 이벤트를 처리한다.
		void handle(const sf::Event& event)
		{
			if (event.type == sf::Event::Closed) {
				request(Action::quit);
				return;
			}
			if (event.type == sf::Event::LostFocus)
				stopStroke();
			if (event.type == sf::Event::Resized && event.size.width && event.size.height) {
				sf::View view(sf::FloatRect(0, 0, 1536, 960));
				const float ratio = static_cast<float>(event.size.width) / event.size.height;
				if (ratio > 1.6f)
					view.setViewport({(1 - 1.6f / ratio) / 2, 0, 1.6f / ratio, 1});
				else
					view.setViewport({0, (1 - ratio / 1.6f) / 2, 1, ratio / 1.6f});
				mWindow.setView(view);
			}
			if (event.type == sf::Event::MouseButtonReleased && mStroke) {
				if (mRectangleStroke)
					updateRange(mWindow.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y}));
				stopStroke(true);
			}
			if (mModal != Modal::none) {
				if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
					mModal = Modal::none;
					mPending = Action::none;
				} else if (event.type == sf::Event::KeyPressed && mModal != Modal::unsaved) {
					if (event.key.code == sf::Keyboard::Enter)
						submitPath();
					else if (event.key.code == sf::Keyboard::BackSpace) {
						if (mReplaceInput)
							mInput.clear();
						else if (!mInput.isEmpty())
							mInput.erase(mInput.getSize() - 1);
						mReplaceInput = false;
					} else if ((event.key.control || event.key.system) && event.key.code == sf::Keyboard::V) {
						if (mReplaceInput)
							mInput.clear();
						mInput += sf::Clipboard::getString();
						mReplaceInput = false;
					} else if ((event.key.control || event.key.system) && event.key.code == sf::Keyboard::C)
						sf::Clipboard::setString(mInput);
					else if ((event.key.control || event.key.system) && event.key.code == sf::Keyboard::A)
						mReplaceInput = true;
				} else if (event.type == sf::Event::TextEntered && mModal != Modal::unsaved && event.text.unicode >= 32 &&
						   event.text.unicode != 127) {
					if (mReplaceInput)
						mInput.clear();
					mReplaceInput = false;
					mInput += event.text.unicode;
				} else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left)
					clickModal(mWindow.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y}));
				return;
			}
			if (event.type == sf::Event::KeyPressed) {
				const auto key = event.key.code;
				if (key == sf::Keyboard::LShift || key == sf::Keyboard::RShift)
					return;
				if (key == sf::Keyboard::Escape && mRectangleStroke) {
					stopStroke();
					return;
				}
				const bool command = event.key.control || event.key.system;
				stopStroke();
				if (command && key == sf::Keyboard::S)
					pathModal(Modal::save);
				else if (command && key == sf::Keyboard::O)
					request(Action::open);
				else if (command && key == sf::Keyboard::N)
					request(Action::newMap);
				else if (command && key == sf::Keyboard::Z) {
					if (event.key.shift)
						mDocument.redo();
					else
						mDocument.undo();
					mValidation = mDocument.validate();
				} else if (command && key == sf::Keyboard::Y) {
					mDocument.redo();
					mValidation = mDocument.validate();
				} else if (key >= sf::Keyboard::Num1 && key <= sf::Keyboard::Num7)
					mSelected = key - sf::Keyboard::Num1;
				else if (key == sf::Keyboard::R)
					mExitDirection = (mExitDirection + 1) % 4;
				else if (key == sf::Keyboard::B)
					mRectangleMode = !mRectangleMode;
				else if (key == sf::Keyboard::G)
					mGrid = !mGrid;
				else if (key == sf::Keyboard::V)
					mValidation = mDocument.validate();
				else if (key == sf::Keyboard::Escape)
					request(Action::quit);
			}
			if (event.type == sf::Event::MouseButtonPressed) {
				const auto pos = mWindow.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
				if (const auto tile = tileAt(pos)) {
					if (event.mouseButton.button == sf::Mouse::Left || event.mouseButton.button == sf::Mouse::Right) {
						mStroke = true;
						mErasing = event.mouseButton.button == sf::Mouse::Right;
						mLast.reset();
						mRectangleStroke = (mRectangleMode || sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) ||
											sf::Keyboard::isKeyPressed(sf::Keyboard::RShift)) &&
										   (mErasing || (mSelected != 4 && mSelected != 5));
						if (mRectangleStroke) {
							mRangeStart = *tile;
							mRangeEnd = *tile;
						} else {
							mDocument.beginStroke();
							apply(*tile);
						}
					}
				} else if (event.mouseButton.button == sf::Mouse::Left) {
					for (int i = 0; i < 7; ++i)
						if (sf::FloatRect(sidebar, 138.f + i * 52, 312, 44).contains(pos))
							mSelected = i;
					if (sf::FloatRect(sidebar, 828, 150, 32).contains(pos))
						mRectangleMode = false;
					if (sf::FloatRect(sidebar + 162, 828, 150, 32).contains(pos))
						mRectangleMode = true;
					if (sf::FloatRect(sidebar, 560, 312, 44).contains(pos))
						pathModal(Modal::save);
					if (sf::FloatRect(sidebar, 614, 312, 44).contains(pos))
						request(Action::open);
					if (sf::FloatRect(sidebar, 668, 312, 44).contains(pos))
						request(Action::newMap);
					if (sf::FloatRect(sidebar, 722, 312, 44).contains(pos)) {
						mDocument.undo();
						mValidation = mDocument.validate();
					}
					if (sf::FloatRect(sidebar, 776, 312, 44).contains(pos)) {
						mDocument.redo();
						mValidation = mDocument.validate();
					}
				}
			}
			if (event.type == sf::Event::MouseMoved && mStroke) {
				const auto pos = mWindow.mapPixelToCoords({event.mouseMove.x, event.mouseMove.y});
				if (mRectangleStroke) {
					updateRange(pos);
					return;
				}
				if (const auto tile = tileAt(pos)) {
					if (!mLast || *mLast != *tile)
						apply(*tile);
				} else
					mLast.reset();
			}
		}

		// 에디터 화면을 그린다.
		void render()
		{
			mWindow.setTitle(std::string("Map Editor") + (mDocument.dirty() ? " * | " : " | ") + mDocument.path().string());
			// Establish a stable logical coordinate system on the first frame too.
			if (mWindow.getView().getSize() == sf::Vector2f(1280, 800))
				mWindow.setView(sf::View(sf::FloatRect(0, 0, 1536, 960)));
			mWindow.clear(sf::Color(21, 26, 33));
			text("MAP EDITOR", 24, 16, 26);
			text("48 x 32 | Drag: fill range | Right-drag: erase | B: mode | G: grid | V: validate", 24, 54, 18);
			const auto name = mDocument.path().empty() ? std::string("Untitled") : mDocument.path().filename().string();
			text(name + (mDocument.dirty() ? "  (unsaved)" : ""), sidebar, 36, 20);
			constexpr std::array<float, 6> correction{0, -90, -90, 180, 180, 0};
			for (int y = 0; y < Map::height; ++y)
				for (int x = 0; x < Map::width; ++x) {
					const float px = gridX + (x + 0.5f) * editorTileSize, py = gridY + (y + 0.5f) * editorTileSize;
					const int grass = (x * 7 + y * 11) % 12;
					sprite(TextureId::grass, {grass % 6 * 16, grass / 6 * 16, 16, 16}, px, py, 1.5f);
					const char value = mDocument.at({x, y});
					if (value == '#') {
						unsigned mask = 0;
						for (unsigned d = 0; d < 4; ++d) {
							const TilePos next{x + directions[d].x, y + directions[d].y};
							if (Map::contains(next) && mDocument.at(next) == '#')
								mask |= 1u << d;
						}
						const auto visual = makeWallVisual(static_cast<std::uint8_t>(mask));
						const auto shape = static_cast<std::size_t>(visual.shape);
						sprite(TextureId::walls, {static_cast<int>(shape) * 16, 0, 16, 16}, px, py, 1.5f,
							   visual.rotationDegrees + correction[shape]);
					} else if (value == 'o')
						sprite(TextureId::fruit, {0, 0, 16, 16}, px, py, 1.5f);
					else if (value == 's')
						sprite(TextureId::lotus, {0, 0, 32, 32}, px, py, 0.75f);
					else if (value == 'P') {
						sprite(TextureId::player, {0, 0, 16, 16}, px, py, 1.5f);
						text("P", px - 5, py - 10, 14, sf::Color::White);
					} else if (value == 'H') {
						panel({px - 11, py - 11, 22, 22}, {60, 69, 100});
						text("H", px - 6, py - 11, 18);
					}
				}
			if (mGrid) {
				sf::VertexArray lines(sf::Lines);
				const sf::Color color(15, 25, 20, 70);
				for (int x = 0; x <= Map::width; ++x) {
					lines.append({{gridX + x * editorTileSize, gridY}, color});
					lines.append({{gridX + x * editorTileSize, gridY + Map::height * editorTileSize}, color});
				}
				for (int y = 0; y <= Map::height; ++y) {
					lines.append({{gridX, gridY + y * editorTileSize}, color});
					lines.append({{gridX + Map::width * editorTileSize, gridY + y * editorTileSize}, color});
				}
				mWindow.draw(lines);
			}
			if (mRectangleStroke && mRangeStart && mRangeEnd) {
				const int left = std::min(mRangeStart->x, mRangeEnd->x), top = std::min(mRangeStart->y, mRangeEnd->y);
				const int width = std::abs(mRangeEnd->x - mRangeStart->x) + 1, height = std::abs(mRangeEnd->y - mRangeStart->y) + 1;
				panel({gridX + left * editorTileSize, gridY + top * editorTileSize, width * editorTileSize, height * editorTileSize},
					  {90, 210, 170, 80}, {135, 255, 205});
				text(std::to_string(width) + " x " + std::to_string(height), sidebar, 864, 17);
			}
			if (mModal == Modal::none && !mRectangleStroke) {
				const auto pos = mWindow.mapPixelToCoords(sf::Mouse::getPosition(mWindow));
				if (const auto tile = tileAt(pos)) {
					const float extent = mSelected == 5 ? 3 * editorTileSize : editorTileSize;
					const float offset = mSelected == 5 ? editorTileSize : 0;
					panel({gridX + tile->x * editorTileSize - offset, gridY + tile->y * editorTileSize - offset, extent, extent},
						  {255, 255, 255, 30}, {255, 255, 255, 220});
					text("Tile: " + std::to_string(tile->x) + ", " + std::to_string(tile->y), sidebar, 860, 17);
				}
			}
			text("PAINT TOOLS", sidebar, 100, 18, {135, 156, 176});
			for (int i = 0; i < 7; ++i)
				button({sidebar, 138.f + i * 52, 312, 44}, brushNames[i], mSelected == i);
			constexpr std::array<const char*, 4> exits{"Up", "Right", "Down", "Left"};
			text(std::string("House exit: ") + exits[mExitDirection] + "   [R]", sidebar, 514, 17);
			button({sidebar, 560, 312, 44}, "Save / Save as   [Cmd/Ctrl S]");
			button({sidebar, 614, 312, 44}, "Open map   [Cmd/Ctrl O]");
			button({sidebar, 668, 312, 44}, "New map   [Cmd/Ctrl N]");
			button({sidebar, 722, 312, 44}, "Undo   [Cmd/Ctrl Z]");
			button({sidebar, 776, 312, 44}, "Redo   [Cmd/Ctrl Y]");
			button({sidebar, 828, 150, 32}, "Brush [B]", !mRectangleMode);
			button({sidebar + 162, 828, 150, 32}, "Range [B]", mRectangleMode);
			text(mValidation, 24, 875, 17, mValidation == "VALID" ? sf::Color(117, 226, 166) : sf::Color(255, 182, 113));
			// Status is separate from validity so saving an invalid draft cannot appear valid.
			const auto shown = mStatus.size() > 155 ? mStatus.substr(0, 152) + "..." : mStatus;
			text(shown, 24, 908, 15);
			if (mModal != Modal::none) {
				panel({0, 0, 1536, 960}, {0, 0, 0, 180});
				panel({370, 330, 760, 250}, {31, 39, 50}, {105, 129, 150});
				if (mModal == Modal::unsaved) {
					text("Unsaved changes", 400, 355, 26);
					text("Save your changes before continuing?", 400, 410, 19);
					button({430, 500, 200, 48}, "Save...");
					button({650, 500, 200, 48}, "Discard");
					button({870, 500, 200, 48}, "Cancel");
				} else {
					text(mModal == Modal::open ? "Open map" : "Save map / draft", 400, 350, 26);
					text("Full path  |  Cmd/Ctrl+A selects all  |  Enter confirms", 400, 390, 16);
					panel({400, 420, 700, 50}, mReplaceInput ? sf::Color(53, 76, 98) : sf::Color(15, 20, 28), {104, 145, 172});
					auto shown = mInput;
					if (shown.getSize() > 68)
						shown = sf::String("...") + shown.substring(shown.getSize() - 65);
					sf::Text pathText(shown, mFont, 17);
					pathText.setPosition(410, 434);
					mWindow.draw(pathText);
					button({750, 500, 150, 48}, "Cancel");
					button({920, 500, 150, 48}, mModal == Modal::open ? "Open" : "Save");
				}
			}
			mWindow.display();
		}
	};
}	 // namespace

// 프로그램 인자를 해석하고 애플리케이션을 실행한다.
int main(int argc, char** argv)
{
	try {
		if (argc > 3)
			throw std::runtime_error("Usage: MapEditor [map-file] [font-file]");
		const auto executable = std::filesystem::absolute(argv[0]);
		const auto project = executable.parent_path().parent_path();
		std::filesystem::path map = project / "Maps/Level01.txt";
		std::filesystem::path font = "/System/Library/Fonts/Supplemental/Arial.ttf";
		int positional = 0;
		for (int i = 1; i < argc; ++i) {
			if (std::string(argv[i]).starts_with("--"))
				throw std::runtime_error("Usage: MapEditor [map-file] [font-file]");
			if (positional++ == 0)
				map = pathFromUtf8(argv[i]);
			else
				font = pathFromUtf8(argv[i]);
		}
		Editor editor(executable, map, font);
		editor.run();
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "MapEditor: " << error.what() << '\n';
		return 1;
	}
}
