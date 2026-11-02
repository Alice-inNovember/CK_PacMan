#include "System/Game.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace pacman {
	namespace {
		constexpr double epsilon = 1e-10;

		// 정지 상태의 이동 기록을 만든다.
		MovementTrace stationary(WorldPosition position, double dt)
		{
			return {position, dt, {{position, position, 0, dt}}, {}};
		}

		// 이동 기록에 구간과 도착 시각을 추가한다.
		void append(MovementTrace& output, const MovementTrace& part, double offset)
		{
			for (auto segment : part.segments) {
				segment.begin += offset;
				segment.end += offset;
				output.segments.push_back(segment);
			}
			for (auto arrival : part.arrivals) {
				arrival.time += offset;
				output.arrivals.push_back(arrival);
			}
		}

		// 두 위치가 충돌 거리 이내인지 확인한다.
		bool touching(WorldPosition a, WorldPosition b, const Map& map)
		{
			double dx = std::abs(a.x - b.x);
			double dy = std::abs(a.y - b.y);
			const TilePos first{static_cast<int>(a.x / Map::tileSize), static_cast<int>(a.y / Map::tileSize)};
			const TilePos second{static_cast<int>(b.x / Map::tileSize), static_cast<int>(b.y / Map::tileSize)};
			constexpr double width = Map::width * Map::tileSize;
			constexpr double height = Map::height * Map::tileSize;
			if (dx > width / 2 && map.step({0, first.y}, {-1, 0}) && map.step({0, second.y}, {-1, 0}))
				dx = width - dx;
			if (dy > height / 2 && map.step({first.x, 0}, {0, -1}) && map.step({second.x, 0}, {0, -1}))
				dy = height - dy;
			return dx * dx + dy * dy <= GameConfig::contactRadius * GameConfig::contactRadius;
		}

	}	 // namespace

	// 맵으로 게임과 탐색 그래프를 초기화한다.
	Game::Game(Map map)
		: mInitialMap(map),
		  mMap(std::move(map)),
		  mGraph(NavigationGraph::build(mMap)),
		  mPlayer(mGraph, mGraph.nodeAt(mMap.playerSpawn()).value()),
		  mGhosts{{{GhostKind::red, Mover(mGraph, mGraph.nodeAt(mMap.house()).value(), Traveler::ghost)},
				   {GhostKind::green, Mover(mGraph, mGraph.nodeAt(mMap.house()).value(), Traveler::ghost)},
				   {GhostKind::blue, Mover(mGraph, mGraph.nodeAt(mMap.house()).value(), Traveler::ghost)}}}
	{
		mPlayerTrace = stationary(mPlayer.mover().position(mGraph), 0);
		for (std::size_t i = 0; i < mGhosts.size(); ++i)
			mGhostTraces[i] = stationary(mGhosts[i].mover.position(mGraph), 0);
	}

	// 해당 고스트의 AI 관측 자료를 구성한다.
	AiContext Game::aiContext(std::size_t index) const
	{
		const auto& actor = mGhosts.at(index);
		return {mMap,
				mGraph,
				mPlayer,
				{actor.mover, actor.condition, actor.target, actor.allowed, actor.blockedEdge, actor.relocation},
				mRound.phase,
				ghostReleased(index),
				index};
	}

	// 발생한 게임 이벤트를 가져오고 비운다.
	std::vector<GameEvent> Game::takeEvents()
	{
		auto result = std::move(mEvents);
		mEvents.clear();
		return result;
	}

	// 초기 맵과 라운드로 재시작한다.
	void Game::restart()
	{
		*this = Game(mInitialMap);
	}

	// 디버그 조작으로 목숨을 추가한다.
	void Game::addDebugLife()
	{
		mRound.addDebugLife();
	}

	// 라운드가 종료 상태인지 확인한다.
	bool Game::terminal() const
	{
		return mRound.phase == RoundPhase::victory || mRound.phase == RoundPhase::gameOver ||
			   mRound.phase == RoundPhase::error;
	}

	// 플레이어의 다음 이동 방향을 예약한다.
	void Game::queueDirection(Direction direction)
	{
		if (mRound.phase == RoundPhase::ready || mRound.phase == RoundPhase::playing)
			mPlayer.queueDirection(direction);
	}

	// 고스트가 현재 이동과 충돌에 참여하는지 확인한다.
	bool Game::ghostActive(std::size_t index) const
	{
		return mRound.phase == RoundPhase::recovering || mGhosts[index].condition == GhostCondition::eaten ||
			   mRound.activeTime + epsilon >= index * GameConfig::ghostReleaseInterval;
	}

	// 고스트의 출격 여부를 확인한다.
	bool Game::ghostReleased(std::size_t index) const
	{
		return mRound.phase == RoundPhase::playing && ghostActive(index);
	}

	// 현재 조건에 따른 고스트 속도를 반환한다.
	double Game::ghostSpeed(std::size_t index) const
	{
		const auto& ghost = mGhosts.at(index);
		if (mRound.phase == RoundPhase::recovering || ghost.condition == GhostCondition::eaten)
			return GameConfig::returnSpeed;
		const double normal = ghost.kind == GhostKind::red ? GameConfig::redSpeed : GameConfig::ghostSpeed;
		return ghost.condition == GhostCondition::frightened ? normal * GameConfig::frightenedFactor : normal;
	}

	// Targets are supplied by the AI layer. nullopt intentionally keeps a normal ghost still.
	void Game::step(const GhostTargets& targets, NodePlanner nodePlanner)
	{
		for (const auto& target : targets)
			if (target.node)
				mGraph.at(*target.node);
		if (terminal()) {
			mPlayerTrace = stationary(mPlayer.mover().position(mGraph), GameConfig::fixedStep);
			for (std::size_t i = 0; i < mGhosts.size(); ++i)
				mGhostTraces[i] = stationary(mGhosts[i].mover.position(mGraph), GameConfig::fixedStep);
			return;
		}
		tick(targets, nodePlanner);
	}

	// 한 고정 업데이트의 게임 규칙을 처리한다.
	void Game::tick(const GhostTargets& targets, const NodePlanner& nodePlanner)
	{
		double dt = GameConfig::fixedStep;
		mPlayerTrace = {mPlayer.mover().position(mGraph), dt, {}, {}};
		for (std::size_t i = 0; i < mGhosts.size(); ++i)
			mGhostTraces[i] = {mGhosts[i].mover.position(mGraph), dt, {}, {}};
		if (mRound.phase == RoundPhase::ready) {
			const double waiting = std::min(dt, mRound.readyRemaining);
			append(mPlayerTrace, stationary(mPlayer.mover().position(mGraph), waiting), 0);
			for (std::size_t i = 0; i < mGhosts.size(); ++i)
				append(mGhostTraces[i], stationary(mGhosts[i].mover.position(mGraph), waiting), 0);
			mRound.readyRemaining = std::max(0.0, mRound.readyRemaining - waiting);
			dt -= waiting;
			if (mRound.readyRemaining < epsilon)
				mRound.phase = RoundPhase::playing;
		}
		if (dt > epsilon && !terminal()) {
			const bool playing = mRound.phase == RoundPhase::playing;
			simulate(dt, targets, nodePlanner);
			if (playing && mRound.phase == RoundPhase::playing) {
				collectAtPlayer();
				if (terminal())
					return;
				resolveContacts();
				mRound.finishCollection(mMap.remainingCollectibles());
			}
		}
	}

	// 다음 시간 경계까지 이동 가능한 시간을 구한다.
	double Game::movementHorizon(double left, const std::array<std::optional<NodeId>, 3>& next) const
	{
		double horizon = left;
		for (std::size_t i = 0; i < mGhosts.size(); ++i) {
			if (!ghostActive(i))
				continue;
			const auto& mover = mGhosts[i].mover;
			if (mover.nextNode())
				horizon = std::min(horizon, mover.remainingPixels() / ghostSpeed(i));
			else if (next[i])
				horizon =
					std::min(horizon, mGraph.edgeCost(mover.currentNode(), *next[i]) * Map::tileSize / ghostSpeed(i));
		}
		if (mRound.phase == RoundPhase::playing) {
			if (mRound.powerRemaining > epsilon)
				horizon = std::min(horizon, mRound.powerRemaining);
			for (std::size_t i = 1; i < mGhosts.size(); ++i) {
				const double release = i * GameConfig::ghostReleaseInterval - mRound.activeTime;
				if (release > epsilon)
					horizon = std::min(horizon, release);
			}
		}
		return horizon;
	}

	// 다음 이동 노드와 이동 시간을 계획한다.
	Game::MovementPlan Game::planMovement(double left, const GhostTargets& targets)
	{
		MovementPlan plan{};
		for (std::size_t i = 0; i < mGhosts.size(); ++i) {
			auto& ghost = mGhosts[i];
			if (const auto center = ghost.mover.centerNode(); ghostActive(i) && center)
				plan.next[i] = ghost.nextNode(mMap, mGraph, mPlayer.mover(), mRound.phase == RoundPhase::recovering,
											  *center, targets[i]);
		}
		plan.duration = movementHorizon(left, plan.next);
		return plan;
	}

	// 계획된 시간만큼 배우를 이동한다.
	void Game::move(const MovementPlan& plan, double offset)
	{
		const auto playerMotion = mRound.phase == RoundPhase::recovering
									  ? stationary(mPlayer.mover().position(mGraph), plan.duration)
									  : mPlayer.update(mGraph, plan.duration);
		append(mPlayerTrace, playerMotion, offset);
		for (std::size_t i = 0; i < mGhosts.size(); ++i) {
			auto& ghost = mGhosts[i];
			const auto motion = ghostActive(i) ? ghost.mover.advance(mGraph, ghostSpeed(i), plan.duration,
																	 [&](NodeId) { return plan.next[i]; })
											   : stationary(ghost.mover.position(mGraph), plan.duration);
			append(mGhostTraces[i], motion, offset);
		}
		for (const auto& ghost : mGhosts)
			if (ghost.routeUnreachable) {
				mRound.phase = RoundPhase::error;
				mRound.error = "ghost route is unreachable";
			}
	}

	// 플레이어와 고스트의 접촉을 처리한다.
	void Game::resolveContacts()
	{
		const auto playerPosition = mPlayer.mover().position(mGraph);
		bool lethal = false;
		for (std::size_t i = 0; i < mGhosts.size(); ++i) {
			if (!ghostActive(i) || mGhosts[i].condition == GhostCondition::eaten)
				continue;
			const auto b = mGhosts[i].mover.position(mGraph);
			if (!touching(playerPosition, b, mMap))
				continue;
			if (mGhosts[i].condition == GhostCondition::frightened) {
				mRound.captureGhost(mGhosts[i], b, mEvents);
			} else
				lethal = true;
		}
		if (lethal)
			mRound.die(mPlayer, mGhosts);
	}

	// 시간 경계마다 이동을 나누어 진행한다.
	void Game::simulate(double dt, const GhostTargets& targets, const NodePlanner& nodePlanner)
	{
		const auto home = mGraph.nodeAt(mMap.house()).value();
		double left = dt;
		while (left > epsilon && !terminal()) {
			const double offset = GameConfig::fixedStep - left;
			if (mRound.phase == RoundPhase::playing)
				mRound.reviveGhosts(mGhosts, home);
			GhostTargets currentTargets = targets;
			if (nodePlanner && mRound.phase == RoundPhase::playing)
				for (std::size_t i = 0; i < mGhosts.size(); ++i)
					if (ghostActive(i) && mGhosts[i].mover.centerNode())
						currentTargets[i] = nodePlanner(aiContext(i));
			const auto plan = planMovement(left, currentTargets);
			const double eventTime = plan.duration;
			move(plan, offset);
			left = std::max(0.0, left - eventTime);
			if (mRound.phase == RoundPhase::recovering) {
				mRound.recoveryTime += eventTime;
				const bool allHome = std::all_of(mGhosts.begin(), mGhosts.end(), [&](const GhostActor& ghost) {
					return ghost.mover.centerNode() == home;
				});
				if (allHome) {
					mRound.finishRecovery(mGhosts);
					break;
				}
				continue;
			}
			mRound.advancePower(eventTime);
			mRound.reviveGhosts(mGhosts, home);
			mRound.expirePower(mGhosts);
		}
		const double finish = GameConfig::fixedStep - left;
		if (left > epsilon) {
			append(mPlayerTrace, stationary(mPlayer.mover().position(mGraph), left), finish);
			for (std::size_t i = 0; i < mGhosts.size(); ++i)
				append(mGhostTraces[i], stationary(mGhosts[i].mover.position(mGraph), left), finish);
		}
	}

	// 플레이어의 현재 칸에서 수집물을 처리한다.
	void Game::collectAtPlayer()
	{
		const auto position = mPlayer.mover().position(mGraph);
		const TilePos tile{static_cast<int>(position.x / Map::tileSize), static_cast<int>(position.y / Map::tileSize)};
		const auto kind = mMap.at(tile).collectible;
		if (kind == Collectible::phaseShuffle && !shuffleGhosts()) {
			mRound.phase = RoundPhase::error;
			mRound.error = "phase shuffle has no safe assignment for all three ghosts";
			return;
		}
		mRound.applyCollectible(mGhosts, mMap.collect(tile));
	}

	// 고스트를 서로 다른 안전 노드로 재배치한다.
	bool Game::shuffleGhosts()
	{
		const auto distance = mPlayer.mover().flooredDistancesToNodes(mGraph);
		const auto home = mGraph.nodeAt(mMap.house()).value();
		const auto exit = mGraph.nodeAt(mMap.houseExit()).value();
		const auto spawn = mGraph.nodeAt(mMap.playerSpawn()).value();
		std::array<std::vector<NodeId>, 3> candidates;
		for (std::size_t i = 0; i < mGhosts.size(); ++i) {
			for (std::size_t n = 0; n < mGraph.nodes().size(); ++n) {
				const NodeId node{n};
				if (distance[n] < GameConfig::shuffleSafeDistance || node == home || node == exit || node == spawn ||
					node == mGhosts[i].mover.currentNode() || node == mGhosts[i].mover.planningNode())
					continue;
				candidates[i].push_back(node);
			}
			for (std::size_t remaining = candidates[i].size(); remaining > 1; --remaining)
				std::swap(candidates[i][remaining - 1], candidates[i][std::rand() % remaining]);
		}
		std::array<NodeId, 3> chosen{};
		const auto assign = [&](auto&& self, std::size_t i) -> bool {
			if (i == chosen.size())
				return true;
			for (const auto node : candidates[i]) {
				if (std::find(chosen.begin(), chosen.begin() + i, node) != chosen.begin() + i)
					continue;
				chosen[i] = node;
				if (self(self, i + 1))
					return true;
			}
			return false;
		};
		if (!assign(assign, 0))
			return false;
		for (std::size_t i = 0; i < mGhosts.size(); ++i) {
			mGhosts[i].mover = Mover(mGraph, chosen[i], Traveler::ghost);
			++mGhosts[i].relocation;
			const auto position = mGhosts[i].mover.position(mGraph);
			const double elapsed = mGhostTraces[i].segments.empty() ? 0 : mGhostTraces[i].segments.back().end;
			mGhostTraces[i] = {position, GameConfig::fixedStep, {}, {}};
			if (elapsed > 0)
				append(mGhostTraces[i], stationary(position, elapsed), 0);
			mGhosts[i].path.clear();
		}
		return true;
	}
}	 // namespace pacman
