#include "Rendering/TextureStore.h"

#include <cmath>
#include <stdexcept>
#include "System/GameConfig.h"

namespace pacman {
	// 게임의 텍스처를 읽고 보관한다.
	TextureStore::TextureStore(const std::filesystem::path& root)
	{
		struct Definition
		{
			TextureId id;
			const char* file;
			unsigned width;
			unsigned height;
		};

		constexpr std::array<Definition, 13> definitions{{{TextureId::red, "EnemyRed.png", 256, 32},
														  {TextureId::green, "EnemyGreen.png", 256, 32},
														  {TextureId::blue, "EnemyBlue.png", 256, 32},
														  {TextureId::fruit, "Fruit.png", 80, 16},
														  {TextureId::lotus, "Lotus.png", 32, 32},
														  {TextureId::sparke, "Sparke.png", 256, 32},
														  {TextureId::walls, "Walls.png", 96, 16},
														  {TextureId::gems, "Gems.png", 96, 16},
														  {TextureId::player, "Player.png", 64, 48},
														  {TextureId::playerBlue, "PlayerBlue.png", 64, 48},
														  {TextureId::playerRed, "PlayerRed.png", 64, 48},
														  {TextureId::grass, "Grass.png", 96, 32},
														  {TextureId::returnGhost, "Return.png", 48, 16}}};
		for (std::size_t i = 0; i < definitions.size(); ++i) {
			const auto& definition = definitions[i];
			const auto path = root / definition.file;
			sf::Image image;
			if (!image.loadFromFile(path.string()))
				throw std::runtime_error("cannot load asset: " + path.string());
			if (image.getSize() != sf::Vector2u(definition.width, definition.height))
				throw std::runtime_error("unexpected sheet dimensions: " + path.string());
			const auto index = static_cast<std::size_t>(definition.id);
			if (!mTextures[index].loadFromImage(image))
				throw std::runtime_error("cannot create texture: " + path.string());
			mTextures[index].setSmooth(false);
			if (i >= 3)
				continue;
			for (unsigned y = 0; y < image.getSize().y; ++y) {
				for (unsigned x = 0; x < image.getSize().x; ++x) {
					auto color = image.getPixel(x, y);
					const auto brighten = [](sf::Uint8 value) {
						return static_cast<sf::Uint8>(std::lround(value + (255 - value) * GameConfig::frightenedWhiteness));
					};
					image.setPixel(x, y, {brighten(color.r), brighten(color.g), brighten(color.b), color.a});
				}
			}
			if (!mTextures[7 + i].loadFromImage(image))
				throw std::runtime_error("cannot create frightened texture");
			mTextures[7 + i].setSmooth(false);
		}
	}

	// 지정한 텍스처를 반환한다.
	const sf::Texture& TextureStore::at(TextureId id) const
	{
		return mTextures.at(static_cast<std::size_t>(id));
	}
}	 // namespace pacman
