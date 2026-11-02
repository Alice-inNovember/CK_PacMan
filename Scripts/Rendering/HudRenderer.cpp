#include "Rendering/HudRenderer.h"
#include <iomanip>
#include <sstream>
#include "Rendering/UiStyle.h"

namespace pacman {
	namespace {
		// 라운드 상태의 표시 이름을 반환한다.
		const char* phaseName(RoundPhase phase)
		{
			switch (phase) {
				case RoundPhase::ready:
					return "GET READY";
				case RoundPhase::playing:
					return "PLAYING";
				case RoundPhase::recovering:
					return "RETURNING TO HOUSE";
				case RoundPhase::victory:
					return "VICTORY";
				case RoundPhase::gameOver:
					return "GAME OVER";
				case RoundPhase::error:
					return "GAME ERROR";
			}
			return "";
		}

		// 방향의 표시 이름을 반환한다.
		const char* directionName(Direction direction)
		{
			constexpr std::array<const char*, 4> names{"Up", "Right", "Down", "Left"};
			return names[static_cast<std::size_t>(direction)];
		}

		// 디버그 표시용 텍스트를 구성한다.
		void label(sf::RenderTarget& target, const sf::Font& font, const std::string& value, float x, float y, unsigned size,
				   sf::Color color = sf::Color::White)
		{
			sf::Text text(value, font, size);
			text.setPosition(x, y);
			text.setFillColor(color);
			target.draw(text);
		}

	}	 // namespace

	// HUD를 그린다.
	void HudRenderer::draw(sf::RenderTarget& target, const sf::Font& font, const GamePresentation& view, bool paused) const
	{
		label(target, font, "PACMAN", ui::left, 28, ui::emphasis, {243, 218, 130});
		label(target, font, "SCORE", ui::left, 82, ui::small, {150, 170, 183});
		label(target, font, std::to_string(view.score), ui::left, 102, ui::emphasis);
		label(target, font, "LIVES  " + std::to_string(view.lives), ui::left, 164, ui::medium);
		label(target, font, "REMAINING  " + std::to_string(view.remaining), ui::left, 200, ui::medium);
		std::ostringstream power;
		power << std::fixed << std::setprecision(1) << view.powerRemaining;
		label(target, font, "POWER  " + power.str() + " s", ui::left, 236, ui::medium);
		label(target, font, "CONTROLS", ui::left, 326, ui::small, {150, 170, 183});
		label(target, font, "Arrows: move    R: restart", ui::left, 350, ui::small);
		label(target, font, "Space: pause    F1: AI debug", ui::left, 374, ui::small);
		label(target, font, "Esc: exit", ui::left, 398, ui::small);
		label(target, font, std::string("Direction: ") + directionName(view.player.direction), ui::left, 434, ui::small);
		label(target, font, std::string("Queued: ") + (view.queuedDirection ? directionName(*view.queuedDirection) : "none"), ui::left, 458,
			  ui::small);
		const std::string phase = paused ? "PAUSED" : phaseName(view.phase);
		label(target, font, phase, ui::left, 492, ui::medium, {247, 202, 115});
		if (view.phase == RoundPhase::error)
			label(target, font, view.error, 24, 1040, ui::small, {255, 140, 140});
	}

	// 디버그 목숨 버튼의 영역을 반환한다.
	sf::FloatRect HudRenderer::debugLifeButtonBounds()
	{
		return {ui::left, 278, 220, 36};
	}

	// 디버그 목숨 버튼을 그린다.
	void HudRenderer::drawDebugLifeButton(sf::RenderTarget& target, const sf::Font& font, RoundPhase phase) const
	{
		const bool enabled = phase != RoundPhase::victory && phase != RoundPhase::error;
		const auto bounds = debugLifeButtonBounds();
		// 에디터 버튼을 그린다.
		sf::RectangleShape button({bounds.width, bounds.height});
		button.setPosition(bounds.left, bounds.top);
		button.setFillColor(enabled ? sf::Color(48, 92, 70) : sf::Color(55, 55, 55));
		button.setOutlineThickness(1);
		button.setOutlineColor(enabled ? sf::Color(130, 220, 160) : sf::Color(100, 100, 100));
		target.draw(button);
		label(target, font, "+1 LIFE", bounds.left + 62, bounds.top + 4, ui::medium, enabled ? sf::Color::White : sf::Color(140, 140, 140));
	}

	// 일시정지와 게임 결과 오버레이를 그린다.
	void HudRenderer::drawOverlay(sf::RenderTarget& target, const sf::Font& font, const GamePresentation& view) const
	{
		if (view.phase == RoundPhase::victory || view.phase == RoundPhase::gameOver) {
			label(target, font, phaseName(view.phase), 610, 470, ui::emphasis, {255, 240, 170});
			label(target, font, "Press R to start again", 610, 530, ui::medium);
		}
	}
}	 // namespace pacman
