#include "Rendering/SpriteAnimator.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace pacman {
	// 스프라이트 애니메이션의 프레임과 재생 조건을 설정한다.
	SpriteAnimator::SpriteAnimator(int width, int height, int count, double seconds, bool loop)
		: mWidth(width), mHeight(height), mCount(count), mFrameSeconds(seconds), mLoop(loop)
	{
		if (width <= 0 || height <= 0 || count <= 0 || !std::isfinite(seconds) || seconds <= 0)
			throw std::invalid_argument("invalid animation definition");
	}

	// 시간에 따라 상태를 갱신한다.
	void SpriteAnimator::update(double dt)
	{
		if (!std::isfinite(dt) || dt < 0)
			throw std::invalid_argument("invalid animation elapsed time");
		const double duration = mCount * mFrameSeconds;
		mElapsed = mLoop ? std::fmod(mElapsed + std::fmod(dt, duration), duration) : std::min(duration, mElapsed + dt);
	}

	// 현재 애니메이션 프레임을 반환한다.
	sf::IntRect SpriteAnimator::frame() const
	{
		const int index = std::min(mCount - 1, static_cast<int>(mElapsed / mFrameSeconds));
		return {index * mWidth, 0, mWidth, mHeight};
	}

	// 애니메이션 재생이 끝났는지 확인한다.
	bool SpriteAnimator::finished() const
	{
		return !mLoop && mElapsed >= mCount * mFrameSeconds;
	}
}	 // namespace pacman
