#pragma once

#include "Rendering/GamePresentation.h"
#include "Rendering/SpriteAnimator.h"
#include "Rendering/TextureStore.h"

namespace pacman {

	class AssetRenderer
	{
	   public:
		AssetRenderer(const TextureStore& textures, const Map& map);
		void update(double dt, const Map& map);
		void drawMapAssets(sf::RenderTarget& target, const Map& map) const;
		void drawGhost(sf::RenderTarget& target, GhostKind kind, WorldPosition position, Direction direction, GhostCondition condition);
		void drawGhost(sf::RenderTarget& target, const Map& map, const GhostPresentation& ghost);
		void drawPlayer(sf::RenderTarget& target, WorldPosition position, Direction direction, bool powered, bool visible = true) const;
		void drawPlayer(sf::RenderTarget& target, const GamePresentation& view) const;
		void addSparke(WorldPosition position);
		void consumeEvents(const std::vector<GameEvent>& events);
		void drawEffects(sf::RenderTarget& target) const;

	   private:
		struct Effect
		{
			WorldPosition position;
			SpriteAnimator animation{32, 32, 8, 0.1, false};
		};

		struct HighlightParticle
		{
			double phase = 0;
			int dx = 0, dy = 0;
		};

		struct Highlight
		{
			TilePos tile;
			std::array<HighlightParticle, 3> particles;
		};

		void initializeItemKinds();
		void initializeHighlights(const Map& map);
		void buildFloor();
		void buildWalls(const Map& map);
		void appendWall(const Map& map, TilePos tile);
		void updateHighlights(double dt, const Map& map);
		void drawCollectibles(sf::RenderTarget& target, const Map& map) const;
		void drawHighlights(sf::RenderTarget& target, WorldPosition center, TilePos tile) const;

		const TextureStore& mTextures;
		sf::Texture mFloorTexture;
		sf::VertexArray mWalls{sf::Quads};
		SpriteAnimator mPlayerAnimation{16, 16, 4, 0.15, true};
		SpriteAnimator mPowerFlash{1, 1, 2, 0.12, true};
		std::vector<Highlight> mHighlights;
		std::array<int, Map::width * Map::height> mHighlightIndex;
		SpriteAnimator mEnemyAnimation{32, 32, 8, 0.1, true};
		std::array<int, Map::width * Map::height> mFruitKinds{};
		std::array<int, Map::width * Map::height> mGemKinds{};
		std::array<bool, 3> mFacingLeft{};
		std::vector<Effect> mEffects;
	};
}
