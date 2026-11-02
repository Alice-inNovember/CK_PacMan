#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <filesystem>

namespace pacman {

	enum class TextureId
	{
		red,
		green,
		blue,
		fruit,
		lotus,
		sparke,
		walls,
		brightRed,
		brightGreen,
		brightBlue,
		gems,
		player,
		playerBlue,
		playerRed,
		grass,
		returnGhost,
		count
	};

	class TextureStore
	{
	   public:
		explicit TextureStore(const std::filesystem::path& root);
		const sf::Texture& at(TextureId id) const;

	   private:
		std::array<sf::Texture, static_cast<std::size_t>(TextureId::count)> mTextures;
	};
}
