#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include "AI/GhostController.h"
#include "Rendering/AssetRenderer.h"
#include "Rendering/HudRenderer.h"
#include "System/Debug.h"
#include "System/Game.h"

namespace {
	using namespace pacman;

	struct LaunchOptions
	{
		std::filesystem::path project;
		std::filesystem::path mapFile;
		std::filesystem::path fontFile;
	};

	// 실행 인자에서 맵과 폰트 경로를 결정한다.
	LaunchOptions parseOptions(int argc, char** argv)
	{
		const auto project = std::filesystem::absolute(argv[0]).parent_path().parent_path();
		LaunchOptions options{project, project / "Maps/Level01.txt", project / "Assets/Hud.ttf"};
		if (!std::filesystem::exists(options.fontFile))
			options.fontFile = "/System/Library/Fonts/Supplemental/Arial.ttf";
		if (argc > 3)
			throw std::runtime_error("Usage: PacMan [map-file] [font-file]");
		for (int i = 1; i < argc; ++i) {
			const std::string arg = argv[i];
			if (arg.starts_with("--"))
				throw std::runtime_error("Usage: PacMan [map-file] [font-file]");
			if (i == 1)
				options.mapFile = arg;
			else
				options.fontFile = arg;
		}
		return options;
	}

	class GameApplication
	{
	   public:
		// 맵, 게임 창과 렌더러를 초기화한다.
		explicit GameApplication(const LaunchOptions& options)
			: mGame(Map::loadFromFile(options.mapFile)),
			  mWindow(sf::VideoMode(1920, 1080), "PacMan", sf::Style::Titlebar | sf::Style::Close),
			  mTextures(options.project / "Assets"),
			  mRenderer(std::make_unique<AssetRenderer>(mTextures, mGame.map())),
			  mWorld(sf::FloatRect(0, 0, Map::width * Map::tileSize, Map::height * Map::tileSize)),
			  mHud(sf::FloatRect(0, 0, 1920, 1080))
		{
			mWindow.setFramerateLimit(60);
			mWindow.setKeyRepeatEnabled(false);
			if (!mFont.loadFromFile(options.fontFile.string()))
				throw std::runtime_error("Cannot load HUD font: " + options.fontFile.string());
			mWorld.setViewport(sf::FloatRect(24.f / 1920, 28.f / 1080, 1536.f / 1920, 1024.f / 1080));
			mFrameClock.restart();
		}

		// 창이 열려 있는 동안 이벤트 처리와 화면 갱신을 반복한다.
		void run()
		{
			while (mWindow.isOpen()) {
				pollEvents();
				if (!mWindow.isOpen())
					break;
				updateFrame();
				renderFrame();
			}
		}

	   private:
		Game mGame;
		sf::RenderWindow mWindow;
		sf::Font mFont;
		TextureStore mTextures;
		GhostController mController;
		std::unique_ptr<AssetRenderer> mRenderer;
		sf::Clock mFrameClock;
		double mAccumulator = 0;
		Debug mDebug;
		HudRenderer mHudRenderer;
		bool mPaused = false, mFocused = true;
		sf::View mWorld, mHud;

		// 창 이벤트를 읽고 입력을 처리한다.
		void pollEvents()
		{
			sf::Event event{};
			while (mWindow.pollEvent(event)) {
				switch (event.type) {
					case sf::Event::Closed:
						mWindow.close();
						break;
					case sf::Event::LostFocus:
						mFocused = false;
						break;
					case sf::Event::GainedFocus:
						mFocused = true;
						break;
					case sf::Event::MouseButtonPressed:
						if (event.mouseButton.button == sf::Mouse::Left && mDebug.enabled() &&
							HudRenderer::debugLifeButtonBounds().contains(
								mWindow.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y}, mHud)))
							mGame.addDebugLife();
						break;
					case sf::Event::KeyPressed:
						handleKey(event.key.code);
						break;
					default:
						break;
				}
			}
		}

		// 게임 키 입력을 해당 동작으로 연결한다.
		void handleKey(sf::Keyboard::Key key)
		{
			switch (key) {
				case sf::Keyboard::Escape:
					mWindow.close();
					break;
				case sf::Keyboard::F1:
					mDebug.toggle();
					break;
				case sf::Keyboard::Space:
					mPaused = !mPaused;
					break;
				case sf::Keyboard::Up:
					mGame.queueDirection(Direction::up);
					break;
				case sf::Keyboard::Right:
					mGame.queueDirection(Direction::right);
					break;
				case sf::Keyboard::Down:
					mGame.queueDirection(Direction::down);
					break;
				case sf::Keyboard::Left:
					mGame.queueDirection(Direction::left);
					break;
				case sf::Keyboard::R:
					restartGame();
					break;
				default:
					break;
			}
		}

		// 게임과 표시 상태를 초기화한다.
		void restartGame()
		{
			mGame.restart();
			mDebug.invalidateMap();
			mController.reset();
			mRenderer = std::make_unique<AssetRenderer>(mTextures, mGame.map());
			mAccumulator = 0;
			mPaused = false;
			mFrameClock.restart();
		}

		// 프레임 시간을 누적해 고정 업데이트를 실행한다.
		void updateFrame()
		{
			const double measured = mFrameClock.restart().asSeconds();
			mDebug.updateFrameTime(measured);
			if (mPaused || !mFocused)
				return;
			const double dt = std::min(measured, 0.25);
			mAccumulator += dt;
			while (mAccumulator + 1e-10 >= GameConfig::fixedStep) {
				updateGame();
				mAccumulator = std::max(0.0, mAccumulator - GameConfig::fixedStep);
			}
			mRenderer->update(dt, mGame.map());
		}

		// 고스트 목표와 게임의 고정 틱을 진행한다.
		void updateGame()
		{
			const auto targets = mController.advance(mGame, GameConfig::fixedStep);
			mGame.step(targets, [this](const AiContext& context) { return mController.planAtNode(context); });
			mRenderer->consumeEvents(mGame.takeEvents());
		}

		// 게임 월드와 HUD를 한 프레임 그린다.
		void renderFrame()
		{
			const auto view = GamePresentation(mGame, mAccumulator / GameConfig::fixedStep);
			std::optional<DebugView> debug;
			if (mDebug.enabled())
				debug.emplace(mGame, mController);
			mWindow.clear(sf::Color(20, 24, 30));
			drawWorld(view, debug);
			mWindow.setView(mHud);
			mHudRenderer.draw(mWindow, mFont, view, mPaused || !mFocused);
			if (debug) {
				mDebug.drawHud(mWindow, mFont, *debug);
				mHudRenderer.drawDebugLifeButton(mWindow, mFont, view.phase);
			}
			mHudRenderer.drawOverlay(mWindow, mFont, view);
			mWindow.display();
		}

		// 월드 뷰에 맵과 배우, 효과를 그린다.
		void drawWorld(const GamePresentation& view, const std::optional<DebugView>& debug)
		{
			mWindow.setView(mWorld);
			mRenderer->drawMapAssets(mWindow, view.map);
			for (const auto& ghost : view.ghosts)
				mRenderer->drawGhost(mWindow, view.map, ghost);
			mRenderer->drawPlayer(mWindow, view);
			mRenderer->drawEffects(mWindow);
			if (debug)
				mDebug.drawWorld(mWindow, mFont, *debug);
		}
	};
}	 // namespace

// 프로그램 인자를 해석하고 애플리케이션을 실행한다.
int main(int argc, char** argv)
{
	try {
		std::srand(static_cast<unsigned>(std::time(nullptr)));
		GameApplication application(parseOptions(argc, argv));
		application.run();
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "PacMan: " << error.what() << '\n';
		return 1;
	}
}
