#include "Rendering/AssetRenderer.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace pacman {
	// 지정한 텍스처 영역을 스프라이트로 그린다.
	void draw(sf::RenderTarget& target, const sf::Texture& texture, sf::IntRect rect, WorldPosition pos, float scale = 1, float angle = 0,
			  bool flip = false)
	{
		sf::Sprite sprite(texture, rect);
		sprite.setOrigin(rect.width / 2.f, rect.height / 2.f);
		sprite.setPosition(static_cast<float>(pos.x), static_cast<float>(pos.y));
		sprite.setScale(flip ? -scale : scale, scale);
		sprite.setRotation(angle);
		target.draw(sprite);
	}

	template <typename DrawActor>
	// 경계 순환을 고려해 배우를 그린다.
	void drawWrappedActor(const Map& map, WorldPosition position, const DrawActor& drawActor)
	{
		position = wrapPosition(position);
		constexpr int width = Map::width * Map::tileSize, height = Map::height * Map::tileSize;
		const TilePos tile{static_cast<int>(position.x / Map::tileSize), static_cast<int>(position.y / Map::tileSize)};
		const bool horizontal = map.step({0, tile.y}, {-1, 0}).has_value();
		const bool vertical = map.step({tile.x, 0}, {0, -1}).has_value();
		for (int x = -1; x <= 1; ++x)
			for (int y = -1; y <= 1; ++y) {
				if (x && (!horizontal || (x < 0 ? position.x < width - 16 : position.x > 16)))
					continue;
				if (y && (!vertical || (y < 0 ? position.y < height - 16 : position.y > 16)))
					continue;
				drawActor(WorldPosition{position.x + x * width, position.y + y * height});
			}
	}

	// 맵과 배우를 그릴 렌더링 자료를 준비한다.
	AssetRenderer::AssetRenderer(const TextureStore& textures, const Map& map) : mTextures(textures)
	{
		initializeItemKinds();
		initializeHighlights(map);
		buildFloor();
		buildWalls(map);
	}

	// 타일별 수집물의 표시 종류를 정한다.
	void AssetRenderer::initializeItemKinds()
	{
		for (auto& fruit : mFruitKinds)
			fruit = std::rand() % 5;
		for (auto& gem : mGemKinds)
			gem = std::rand() % 6;
	}

	// 파워 주변 반짝임의 초기 상태를 만든다.
	void AssetRenderer::initializeHighlights(const Map& map)
	{
		mHighlightIndex.fill(-1);
		for (int i = 0; i < Map::width * Map::height; ++i) {
			const TilePos position{i % Map::width, i / Map::width};
			const auto kind = map.at(position).collectible;
			if (kind != Collectible::powerPellet && kind != Collectible::phaseShuffle)
				continue;
			Highlight highlight{position, {}};
			for (auto& particle : highlight.particles) {
				particle.phase = (std::rand() % 2400) / 1000.0;
				particle.dx = std::rand() % 21 - 10;
				particle.dy = std::rand() % 21 - 10;
			}
			mHighlightIndex[i] = static_cast<int>(mHighlights.size());
			mHighlights.push_back(highlight);
		}
	}

	// 바닥 타일을 텍스처로 만든다.
	void AssetRenderer::buildFloor()
	{
		sf::RenderTexture floor;
		if (!floor.create(Map::width * Map::tileSize, Map::height * Map::tileSize))
			throw std::runtime_error("cannot create grass floor texture");
		floor.clear(sf::Color::Transparent);
		sf::Sprite tile(mTextures.at(TextureId::grass));
		tile.setScale(2, 2);
		for (int y = 0; y < Map::height; ++y) {
			for (int x = 0; x < Map::width; ++x) {
				const int kind = std::rand() % 12;
				tile.setTextureRect({(kind % 6) * 16, (kind / 6) * 16, 16, 16});
				tile.setPosition(static_cast<float>(x * Map::tileSize), static_cast<float>(y * Map::tileSize));
				floor.draw(tile);
			}
		}
		floor.display();
		mFloorTexture = floor.getTexture();
		mFloorTexture.setSmooth(false);
	}

	// 맵의 벽 표시 정점을 구성한다.
	void AssetRenderer::buildWalls(const Map& map)
	{
		for (int y = 0; y < Map::height; ++y)
			for (int x = 0; x < Map::width; ++x)
				if (map.at({x, y}).wall)
					appendWall(map, {x, y});
	}

	// 벽 타일의 표시 정점을 추가한다.
	void AssetRenderer::appendWall(const Map& map, TilePos tile)
	{
		constexpr std::array<float, 6> corrections{0, -90, -90, 180, 180, 0};
		const auto visual = map.wallVisual(tile);
		const auto index = static_cast<std::size_t>(visual.shape);
		sf::Sprite wall(mTextures.at(TextureId::walls), {static_cast<int>(index) * 16, 0, 16, 16});
		wall.setOrigin(8, 8);
		wall.setPosition((tile.x + 0.5f) * Map::tileSize, (tile.y + 0.5f) * Map::tileSize);
		wall.setScale(2, 2);
		wall.setRotation(visual.rotationDegrees + corrections[index]);
		const float left = static_cast<float>(index * 16);
		const std::array<sf::Vector2f, 4> corners{{{0, 0}, {16, 0}, {16, 16}, {0, 16}}};
		for (const auto corner : corners)
			mWalls.append({wall.getTransform().transformPoint(corner), {left + corner.x, corner.y}});
	}

	// 시간에 따라 상태를 갱신한다.
	void AssetRenderer::update(double dt, const Map& map)
	{
		mEnemyAnimation.update(dt);
		updateHighlights(dt, map);
		mPlayerAnimation.update(dt);
		mPowerFlash.update(dt);
		for (auto& effect : mEffects)
			effect.animation.update(dt);
		std::erase_if(mEffects, [](const Effect& effect) { return effect.animation.finished(); });
	}

	// 남은 아이템 주변의 반짝임을 갱신한다.
	void AssetRenderer::updateHighlights(double dt, const Map& map)
	{
		for (auto& highlight : mHighlights) {
			if (map.at(highlight.tile).collectible == Collectible::none)
				continue;
			for (auto& particle : highlight.particles) {
				particle.phase += dt;
				while (particle.phase >= 2.4) {
					particle.phase -= 2.4;
					particle.dx = std::rand() % 21 - 10;
					particle.dy = std::rand() % 21 - 10;
				}
			}
		}
	}

	// 한 타일의 반짝임을 그린다.
	void AssetRenderer::drawHighlights(sf::RenderTarget& target, WorldPosition center, TilePos tile) const
	{
		const auto index = mHighlightIndex[tile.y * Map::width + tile.x];
		if (index < 0)
			return;
		for (const auto& particle : mHighlights[static_cast<std::size_t>(index)].particles) {
			if (particle.phase >= 0.8)
				continue;
			const int frame = std::min(7, static_cast<int>(particle.phase / 0.1));
			draw(target, mTextures.at(TextureId::sparke), {frame * 32, 0, 32, 32}, {center.x + particle.dx, center.y + particle.dy});
		}
	}

	// 바닥과 벽, 수집물을 그린다.
	void AssetRenderer::drawMapAssets(sf::RenderTarget& target, const Map& map) const
	{
		target.draw(sf::Sprite(mFloorTexture));
		target.draw(mWalls, &mTextures.at(TextureId::walls));
		drawCollectibles(target, map);
	}

	// 남은 수집물과 강조 효과를 그린다.
	void AssetRenderer::drawCollectibles(sf::RenderTarget& target, const Map& map) const
	{
		sf::VertexArray gems(sf::Quads);
		const auto flushGems = [&] {
			if (gems.getVertexCount() != 0) {
				target.draw(gems, &mTextures.at(TextureId::gems));
				gems.clear();
			}
		};
		for (int y = 0; y < Map::height; ++y)
			for (int x = 0; x < Map::width; ++x) {
				const auto kind = map.at({x, y}).collectible;
				const WorldPosition pos{(x + 0.5) * Map::tileSize, (y + 0.5) * Map::tileSize};
				if (kind == Collectible::pellet) {
					const float left = static_cast<float>(x * Map::tileSize);
					const float top = static_cast<float>(y * Map::tileSize);
					const float textureLeft = static_cast<float>(mGemKinds[y * Map::width + x] * 16);
					gems.append({{left, top}, {textureLeft, 0}});
					gems.append({{left + Map::tileSize, top}, {textureLeft + 16, 0}});
					gems.append({{left + Map::tileSize, top + Map::tileSize}, {textureLeft + 16, 16}});
					gems.append({{left, top + Map::tileSize}, {textureLeft, 16}});
				} else if (kind == Collectible::powerPellet) {
					flushGems();
					draw(target, mTextures.at(TextureId::fruit), {mFruitKinds[y * Map::width + x] * 16, 0, 16, 16}, pos, 2);
				} else if (kind == Collectible::phaseShuffle) {
					flushGems();
					draw(target, mTextures.at(TextureId::lotus), {0, 0, 32, 32}, pos);
				}
				if (kind == Collectible::powerPellet || kind == Collectible::phaseShuffle)
					drawHighlights(target, pos, {x, y});
			}
		flushGems();
	}

	// 고스트의 현재 외형을 그린다.
	// 고스트의 현재 외형을 그린다.
	void AssetRenderer::drawGhost(sf::RenderTarget& target, GhostKind kind, WorldPosition pos, Direction direction,
								  GhostCondition condition)
	{
		if (condition == GhostCondition::eaten) {
			draw(target, mTextures.at(TextureId::returnGhost), {static_cast<int>(kind) * 16, 0, 16, 16}, pos, 2);
			return;
		}
		const int index = static_cast<int>(kind) + (condition == GhostCondition::frightened ? 7 : 0);
		auto& facingLeft = mFacingLeft[static_cast<std::size_t>(kind)];
		if (direction == Direction::left)
			facingLeft = true;
		else if (direction == Direction::right)
			facingLeft = false;
		draw(target, mTextures.at(static_cast<TextureId>(index)), mEnemyAnimation.frame(), pos, 1, 0, facingLeft);
	}

	void AssetRenderer::drawGhost(sf::RenderTarget& target, const Map& map, const GhostPresentation& ghost)
	{
		drawWrappedActor(map, ghost.actor.position,
						 [&](WorldPosition pos) { drawGhost(target, ghost.kind, pos, ghost.actor.direction, ghost.condition); });
	}

	// 플레이어의 현재 외형을 그린다.
	// 플레이어의 현재 외형을 그린다.
	void AssetRenderer::drawPlayer(sf::RenderTarget& target, WorldPosition position, Direction direction, bool powered, bool visible) const
	{
		if (!visible)
			return;
		TextureId texture = TextureId::player;
		if (powered)
			texture = mPowerFlash.frame().left == 0 ? TextureId::playerBlue : TextureId::playerRed;
		auto rect = mPlayerAnimation.frame();
		switch (direction) {
			case Direction::down:
				rect.top = 0;
				break;
			case Direction::right:
			case Direction::left:
				rect.top = 16;
				break;
			case Direction::up:
				rect.top = 32;
				break;
		}
		draw(target, mTextures.at(texture), rect, position, 2, 0, direction == Direction::left);
	}

	void AssetRenderer::drawPlayer(sf::RenderTarget& target, const GamePresentation& view) const
	{
		drawWrappedActor(view.map, view.player.position,
						 [&](WorldPosition pos) { drawPlayer(target, pos, view.player.direction, view.powered, view.playerVisible); });
	}

	// 지정 위치에 일회성 반짝임을 추가한다.
	void AssetRenderer::addSparke(WorldPosition position)
	{
		mEffects.push_back({position});
	}

	// 게임 이벤트를 표시 효과에 반영한다.
	void AssetRenderer::consumeEvents(const std::vector<GameEvent>& events)
	{
		for (const auto& event : events)
			addSparke(event.position);
	}

	// 진행 중인 포획 효과를 그린다.
	void AssetRenderer::drawEffects(sf::RenderTarget& target) const
	{
		for (const auto& effect : mEffects)
			draw(target, mTextures.at(TextureId::sparke), effect.animation.frame(), effect.position);
	}
}	 // namespace pacman
