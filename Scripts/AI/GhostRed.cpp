#include "AI/GhostRed.h"

namespace pacman {
	// 상시 추격하는 레드 고스트를 만든다.
	GhostRed::GhostRed() : Ghost(GhostStateKind::chase, true) {}

	// 플레이어가 보일 때의 추격 목표를 결정한다.
	GhostTarget GhostRed::chaseVisible(const AiContext& context)
	{
		return {chaseAttachment(context), {}, {}};
	}

	// 순찰 상태의 목표를 결정한다.
	GhostTarget GhostRed::patrolTarget(const AiContext& context)
	{
		return chaseVisible(context);
	}
}	 // namespace pacman
