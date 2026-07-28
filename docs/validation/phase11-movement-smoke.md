# Phase 11 movement smoke

Automated validation uses Godot 4.7.1 stable:

```bash
godot --headless --path godot -s res://tests/run_phase11.gd
```

It verifies typed path preview, fixed-tick traversal, frame-cadence independence,
optional view deletion and reconstruction, path overlay input, cancellation, and
combat-pending safe-boundary behavior.

Interactive release smoke:

1. Run the Phase 11 demo movement boundary.
2. Preview and execute the four-cell route.
3. Vary the rendered frame rate while movement is active.
4. Confirm the overlay follows the preview and the view remains visually smooth.
5. Cancel once, then request combat during a second route.
6. Confirm both operations settle on cell boundaries.

The interactive visual smoothness check remains pending; authoritative behavior and
frame-cadence independence are covered headlessly.
