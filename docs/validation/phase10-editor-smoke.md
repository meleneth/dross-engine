# Phase 10 editor smoke

Use Godot 4.7.1 stable and open the `godot/` project.

1. Open `res://demo/phase10_room.tscn`.
2. Select `GridRegion` and confirm the Dross Hex Grid gizmo is visible.
3. In the Dross Grid dock, click **Bake Geometry** and confirm two cells are reported.
4. Select axial cell `(0, 0)` in the dock and click **Force Traversable**.
5. Click **Bake Geometry** again and confirm the override remains stored.
6. Click **Compile Runtime Map** and confirm two cells and one edge are reported.
7. Run `res://tests/run_phase10.gd`; confirm the runtime overlay contains the same two
   canonical cell keys as the compiled map.
8. Confirm `optional_door_edge` identifies
   `demo:room:0,0,0|demo:room:1,0,0`.

The headless validation exercises the same bake, override, compile, selection, and overlay
operations. The visual viewport check remains an interactive release smoke step.
