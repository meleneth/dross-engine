#pragma once

#include <dross/foundation/quantities.hpp>
#include <dross/hex/path_planner.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace dross {

enum class MovementLifecycleState : std::uint8_t {
  idle,
  traversing,
  blocked,
};

enum class MovementAdvance : std::uint8_t {
  idle,
  in_progress,
  entered_cell,
  completed,
  cancelled,
  blocked,
  combat_boundary,
};

struct MovementConfig {
  std::uint32_t ticks_per_transition;
};

struct MovementPreview {
  bool accepted;
  PathError rejection;
  std::vector<HexPose> path;
  MovementCost cost;
  std::uint64_t duration_ticks;
  std::uint64_t occupancy_revision;
};

class MovementLifecycle {
public:
  MovementLifecycle();
  ~MovementLifecycle();
  MovementLifecycle(MovementLifecycle&&) noexcept;
  MovementLifecycle& operator=(MovementLifecycle&&) noexcept;
  MovementLifecycle(const MovementLifecycle&) = delete;
  MovementLifecycle& operator=(const MovementLifecycle&) = delete;

  [[nodiscard]] bool accept();
  [[nodiscard]] bool finish();
  [[nodiscard]] bool block();
  [[nodiscard]] bool restore(MovementLifecycleState state);
  [[nodiscard]] MovementLifecycleState state() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

struct MovementSnapshot {
  MovementLifecycleState state;
  HexPose pose;
  std::vector<HexPose> path;
  std::size_t next_pose;
  std::uint32_t transition_ticks;
  std::uint64_t expected_occupancy_revision;
  bool cancel_requested;
  bool combat_stop_requested;

  [[nodiscard]] bool operator==(const MovementSnapshot&) const = default;
};

class MovementRuntime {
public:
  MovementRuntime(const CompiledHexMap& map, OccupancyIndex& occupancy, const PathPlanner& planner,
                  const FootprintDefinition& footprint, EntityId entity, HexPose initial_pose,
                  MovementConfig config);

  [[nodiscard]] MovementPreview preview(const HexPose& goal) const;
  [[nodiscard]] bool move_to(const HexPose& goal);
  [[nodiscard]] bool cancel();
  void request_combat_stop();
  [[nodiscard]] MovementAdvance advance(Tick tick);
  [[nodiscard]] MovementSnapshot snapshot() const;
  [[nodiscard]] bool restore(const MovementSnapshot& snapshot);

  [[nodiscard]] const HexPose& pose() const noexcept { return pose_; }
  [[nodiscard]] MovementLifecycleState state() const { return lifecycle_.state(); }

private:
  const CompiledHexMap* map_;
  OccupancyIndex* occupancy_;
  const PathPlanner* planner_;
  const FootprintDefinition* footprint_;
  EntityId entity_;
  HexPose pose_;
  MovementConfig config_;
  MovementLifecycle lifecycle_;
  std::vector<HexPose> path_;
  std::size_t next_pose_{0};
  std::uint32_t transition_ticks_{0};
  std::uint64_t expected_occupancy_revision_{0};
  bool cancel_requested_{false};
  bool combat_stop_requested_{false};
};

} // namespace dross
