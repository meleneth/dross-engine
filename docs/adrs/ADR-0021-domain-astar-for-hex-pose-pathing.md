# ADR-0021: Domain A* for Hex-Pose Pathing

Status: Accepted

## Context

Phase 03 requires deterministic weighted search over an implicit graph whose
vertices are `HexPose` values. Neighbor legality depends on rotated
multi-cell footprints, directional edges, dynamic occupancy, and policy costs.

A spike mapped the asymmetric two-cell rotation fixture to Boost.Graph. Its
A* API requires an explicit vertex/index model or a discovery adapter for the
implicit pose graph. Dross would still need to own neighbor generation,
footprint validation, cost evaluation, predecessor storage, and a priority
queue policy to obtain the required tie order. Boost.Graph's default queue
ordering does not expose the full Dross tie key. The adapter was larger than
the domain search loop and made deterministic ordering less inspectable.

## Decision

Implement domain A* behind `PathPlanner` using standard containers:

- `std::priority_queue` for the open set;
- ordered maps for best costs and predecessors;
- canonical neighbor sorting;
- a conservative zero heuristic until a stronger admissible heuristic is
  proven for policy-dependent rotation and terrain costs.

The queue tie order is estimated total cost, actual cost, anchor cell, facing,
then insertion sequence.

## Consequences

- The initial algorithm is Dijkstra-equivalent but remains an A* implementation
  with an admissible heuristic.
- Search logic stays isolated and can be replaced without changing traversal
  commands.
- Optimality and deterministic tie behavior require exhaustive small-map
  tests.

## Enforcement

- no custom heap implementation;
- no unordered iteration in neighbor or predecessor selection;
- single-cell and multi-cell actors use the same planner;
- occupancy revision is captured in every successful plan.
