#pragma once
#include <memory>
#include "AI/AiContext.h"

namespace pacman {
	class Ghost;
	enum class GhostStateKind
	{
		patrol,
		chase,
		frightened,
		eaten
	};

	struct SearchMemory
	{
		std::optional<NodeId> lastSeenNode;
		double waitElapsed = 0;
		bool searching = false;
	};

	struct StateTransition
	{
		GhostStateKind kind;
		std::optional<NodeId> observedNode;
	};

	class GhostState
	{
	   public:
		virtual ~GhostState() = default;
		virtual GhostStateKind kind() const = 0;
		virtual const char* name() const = 0;
		virtual std::optional<StateTransition> observe(Ghost&, const AiContext&, double) { return {}; }
		virtual GhostTarget planTarget(Ghost&, const AiContext&) { return {}; }

		virtual SearchMemory searchMemory() const { return {}; }

		virtual void resetMemory() {}
	};

	class PatrolState final : public GhostState
	{
	   public:
		GhostStateKind kind() const override { return GhostStateKind::patrol; }

		const char* name() const override { return "Patrol"; }

		std::optional<StateTransition> observe(Ghost&, const AiContext&, double) override;
		GhostTarget planTarget(Ghost&, const AiContext&) override;
	};

	class ChaseState final : public GhostState
	{
	   public:
		explicit ChaseState(std::optional<NodeId> observedNode = {});

		GhostStateKind kind() const override { return GhostStateKind::chase; }

		const char* name() const override { return "Chase"; }

		std::optional<StateTransition> observe(Ghost&, const AiContext&, double) override;
		GhostTarget planTarget(Ghost&, const AiContext&) override;

		SearchMemory searchMemory() const override { return mSearch; }

		void resetMemory() override { mSearch = {}; }

	   private:
		SearchMemory mSearch;
	};

	class FrightenedState final : public GhostState
	{
	   public:
		static std::optional<NodeId> chooseNext(const NavigationGraph&, const Mover&, NodeId);
		GhostStateKind kind() const override { return GhostStateKind::frightened; }

		const char* name() const override { return "Frightened"; }
	};

	class EatenState final : public GhostState
	{
	   public:
		static GhostTarget homeTarget(const Map&, const NavigationGraph&);
		GhostStateKind kind() const override { return GhostStateKind::eaten; }

		const char* name() const override { return "Eaten"; }
	};

	std::unique_ptr<GhostState> createGhostState(const StateTransition&);
}
