# Phase 14 Linux performance baseline

Recorded 2026-07-29 on Linux 6.12.95 x86-64 with an AMD Ryzen 9 7940HS
(16 logical CPUs). The C++ benchmark used the `linux-release` preset. The
integration benchmark used the pinned Godot 4.7.1 release editor and the release
GDExtension.

These measurements are observations, not performance budgets or test gates.
They are intentionally kept out of the aggregate correctness suite.

## Commands

```sh
cmake --preset linux-release
cmake --build --preset linux-release --target dross_benchmarks
./build/linux-release/dross_benchmarks

XDG_DATA_HOME=/tmp/dross-godot-4.7.1/data \
XDG_CONFIG_HOME=/tmp/dross-godot-4.7.1/config \
XDG_CACHE_HOME=/tmp/dross-godot-4.7.1/cache \
/tmp/dross-godot-4.7.1/Godot_v4.7.1-stable_linux.x86_64 \
  --headless --path godot --script res://tests/benchmark_phase14.gd
```

Both runners perform a warmup before measurement. Reported duration samples are
medians and 95th percentiles over 100 batches. Fast Godot operations are batched
to reduce timer quantization.

## Results

| Operation | Work per operation | Median | p95 |
| --- | --- | ---: | ---: |
| Headless fixed tick | Synthetic authoritative world | 0.060 us | 0.060 us |
| Demo path preview | Integrated movement scenario | 21.510 us | 23.820 us |
| Large path preview | 256-cell linear synthetic map, 256-pose result | 5,794.643 us | 6,043.504 us |
| Script callback | One entity-scoped event callback | 5.900 us | 6.200 us |
| Canonical capability hash | Integrated movement scenario | 0.740 us | 0.840 us |
| Integrated save | Canonical vertical-slice state | 5.700 us | 6.000 us |
| Integrated load | Restore the same canonical state | 8.100 us | 8.400 us |
| Editor geometry bake | Two authored cells | 20.000 us | 23.000 us |

The integrated save payload was 692 bytes. The two-cell editor bake corresponds
to approximately 100,000 cells/second at the median and 86,957 cells/second at
the p95 duration; the fixture is deliberately small, so these rates should not
be extrapolated to production-sized rooms.
