# Measuring compute: the upload → dispatch → read round trip

*What a batch of records costs to send through one Vulkan compute pass and back, how to measure it, and
what the numbers are made of.*

*Part of the [build & test guide](../../AGENTS.md). The rules for any timing run are in
[Measuring a frame](measuring-frames.md#rules-for-any-timing-run).*

## The benchmark

`tests/compute` ([the test projects](test-projects.md)) builds a `core::Queue` with one compute pass
and no presentation. The batch is an array of `vec4` records (std430, 16 bytes each). The frame input
carries two buffers: a host-visible staging buffer and a device-local target. The pass copies the
staging buffer into the target, and the shader transforms every record in place. The size lives in
the input, so one compiled queue serves every batch size. Every repetition is read back and verified.

```sh
make -C tests/compute RELEASE=1 -j8
XL_COMPUTE_DEVICE=0 taskset -c 2-9 \
  tests/compute/stappler-build/x86_64-unknown-linux-gnu/release/cc/computetest \
  timings --reps 50 --warmup 5 [--fence export|polled] [--pump-us 100] [--sizes 1000,10000,100000]
```

Each phase is timed from the call to its callback, with the monotonic clock:

| phase | what it covers |
|---|---|
| `upload` | building the input bytes, then `spawnPersistent` of the staging buffer (mapped copy) and of the target |
| `dispatch` | `Loop::runRenderQueue` until its callback: acquiring the queue, recording (copy, barrier, dispatch), submit, the fence, finalizing the frame |

`--fence` picks how `vk::Loop` learns that a submit finished: `export` hands the fence to the looper as a
sync_fd (Linux, on a device that can export; the default), `polled` checks it on the loop's timer,
`config::PresentationSchedulerInterval` - 0.5 ms here, 1 ms on Windows. The `fence=` field of the output
says which path was really taken.
| `read` | `Loop::captureBuffer` until its callback: a transfer task that copies into a host buffer and maps it |

Each cell below is median / p90 / min in microseconds, over 50 repetitions after 5 warm-up runs.

## The pump sets the floor

The CLI thread runs the looper itself: `poll()`, and a sleep of `--pump-us` when nothing was ready.
Every hop from a worker back to the loop thread waits up to one quantum. A frame makes about three
such hops and a capture one or two. With the default 100 µs quantum, a batch of 10³ or 10⁴ records
costs the same ~330 µs and ~160 µs on every device, llvmpipe included. That time is the pump, not the
GPU.

NVIDIA, Linux, polled fences, pinned to cores 2-9:

| `--pump-us` | n | upload | dispatch | read |
|---|---|---|---|---|
| 10 | 10³ | 117 / 510 / 106 | 153 / 624 / 114 | 74 / 79 / 71 |
| 10 | 10⁵ | 717 / 889 / 662 | 666 / 729 / 636 | 585 / 620 / 126 |
| 100 | 10³ | 113 / 119 / 107 | 332 / 335 / 315 | 164 / 168 / 116 |
| 100 | 10⁵ | 677 / 708 / 649 | 335 / 952 / 307 | 164 / 756 / 119 |
| 1000 | 10³ | 123 / 151 / 111 | 2137 / 2144 / 2090 | 1068 / 1074 / 1016 |
| 1000 | 10⁵ | 683 / 757 / 649 | 2136 / 2148 / 2089 | 1066 / 1073 / 1049 |

**Not explained:** on NVIDIA the 10⁵ batch is slower with a 10 µs quantum than with 100 µs (666 µs
against 335 µs for dispatch, 585 µs against 164 µs for read). It reproduced in three runs. It was not
investigated. An executor that runs inside an application waits on the application's own looper, so
its floor will be neither of these.

## Results

Engine `a7560159f` plus the Parallel-10 changes. Release build, `taskset -c 2-9`, `--pump-us 100`,
on a 16-core desktop with nothing else building. An unrelated process held about one core throughout
(load average 3.7-5), so read differences under ~10 % as noise.

### Linux

| device | n | fence | upload | dispatch | read | total |
|---|---|---|---|---|---|---|
| NVIDIA GeForce RTX 4070 Ti SUPER | 10³ | export | 110 / 125 / 107 | 332 / 339 / 311 | 164 / 168 / 115 | 608 / 630 / 571 |
| | | polled | 110 / 124 / 107 | 332 / 339 / 187 | 164 / 169 / 142 | 604 / 627 / 461 |
| | 10⁴ | export | 165 / 172 / 162 | 333 / 335 / 304 | 163 / 167 / 124 | 661 / 675 / 620 |
| | | polled | 165 / 177 / 158 | 334 / 336 / 193 | 164 / 166 / 133 | 663 / 677 / 551 |
| | 10⁵ | export | 704 / 791 / 672 | 335 / 551 / 308 | 165 / 324 / 126 | 1224 / 1835 / 1147 |
| | | polled | 682 / 738 / 661 | 334 / 944 / 291 | 164 / 735 / 121 | 1179 / 2298 / 1119 |
| RADV (Ryzen 9 7950X iGPU) | 10³ | export | 34 / 36 / 26 | 324 / 330 / 275 | 161 / 164 / 123 | 520 / 528 / 437 |
| | | polled | 33 / 43 / 27 | 326 / 331 / 286 | 163 / 166 / 149 | 522 / 534 / 483 |
| | 10⁴ | export | 60 / 64 / 55 | 325 / 330 / 292 | 161 / 165 / 131 | 545 / 558 / 512 |
| | | polled | 61 / 161 / 58 | 326 / 333 / 280 | 162 / 164 / 123 | 550 / 651 / 505 |
| | 10⁵ | export | 386 / 431 / 368 | 499 / 512 / 455 | 479 / 525 / 320 | 1365 / 1669 / 1176 |
| | | polled | 1531 / 1574 / 369 | 949 / 959 / 903 | 781 / 789 / 735 | 3238 / 3315 / 2066 |
| llvmpipe (LLVM 22.1.8) | 10³ | export | 10 / 12 / 9 | 318 / 322 / 166 | 159 / 162 / 29 | 486 / 493 / 208 |
| | | polled | 10 / 11 / 8 | 318 / 323 / 166 | 159 / 161 / 128 | 487 / 493 / 330 |
| | 10⁴ | export | 53 / 55 / 49 | 321 / 325 / 112 | 159 / 162 / 36 | 532 / 538 / 327 |
| | | polled | 56 / 59 / 51 | 322 / 329 / 167 | 161 / 164 / 141 | 538 / 552 / 381 |
| | 10⁵ | export | 485 / 528 / 459 | 749 / 885 / 481 | 164 / 170 / 132 | 1403 / 1593 / 1116 |
| | | polled | 476 / 508 / 457 | 938 / 952 / 787 | 162 / 164 / 158 | 1572 / 1626 / 1414 |

The RADV polled upload at 10⁵ (1531 µs against 386 µs) was not repeated. Every other upload is the same
on both paths, so treat that one cell as an outlier, not as a property of the path.

### Windows (`x86_64-pc-windows-msvc`) under wine-11.16: polled only

| device | n | upload | dispatch | read | total |
|---|---|---|---|---|---|
| NVIDIA GeForce RTX 4070 Ti SUPER | 10³ | 116 / 125 / 110 | 122 / 142 / 109 | 50 / 61 / 44 | 288 / 338 / 269 |
| | 10⁴ | 201 / 211 / 191 | 126 / 1233 / 111 | 50 / 59 / 45 | 379 / 1471 / 353 |
| | 10⁵ | 985 / 1045 / 912 | 1265 / 1292 / 1194 | 1185 / 1221 / 464 | 3432 / 3604 / 3329 |
| AMD Radeon(TM) Graphics (the same iGPU) | 10³ | 35 / 38 / 30 | 1244 / 1269 / 122 | 1209 / 1222 / 205 | 2487 / 2519 / 1349 |
| | 10⁴ | 220 / 235 / 85 | 1252 / 1279 / 1182 | 1212 / 1227 / 266 | 2626 / 2733 / 1615 |
| | 10⁵ | 634 / 655 / 621 | 1259 / 1298 / 1168 | 1220 / 1331 / 1109 | 3125 / 3237 / 2925 |
| llvmpipe (LLVM 22.1.8) | 10³ | 16 / 18 / 13 | 1235 / 1257 / 106 | 49 / 57 / 43 | 1300 / 1333 / 168 |
| | 10⁴ | 83 / 100 / 79 | 1246 / 1269 / 174 | 51 / 58 / 45 | 1384 / 1414 / 320 |
| | 10⁵ | 748 / 767 / 729 | 1250 / 1273 / 1150 | 56 / 1169 / 46 | 2065 / 3171 / 1946 |

## Reading the numbers

- **The export pays where the GPU work passes the floor.** On RADV, 10⁵ records: dispatch 949 → 499 µs,
  read 781 → 479 µs. On llvmpipe: dispatch 938 → 749 µs. A polled fence that is not ready waits for the
  next 0.5 ms tick; an exported one wakes the looper when its fd becomes readable. Below the pump floor,
  batches of 10³ and 10⁴, both paths cost the same.
- **Polled fences are quantized to the scheduler tick.** Under wine that tick is 1 ms, and a fence that
  is not ready at its first check costs about 1.2 ms whatever the batch size.
- **NVIDIA at 10⁵ still sits on the 100 µs pump floor** on both paths.
- **Upload grows with the batch.** The input bytes are built and copied on the calling thread. At 10⁵
  records (1.6 MB), about 0.4–0.7 ms on Linux and 0.6–1.0 ms under wine.
- **`update()` now steps past a fence that is not ready**, where it used to retry the same fence in a
  loop (Parallel-10). Measured under wine on NVIDIA with and without the change: dispatch medians
  127/130/1262 µs against 122/126/1265 µs. No pacing change.

## What was not measured

- A native Windows host. The Windows rows are wine with winevulkan forwarding to the Linux drivers.
- Batches above 10⁵, and records other than `vec4`.
- Upload into an existing buffer, HERE. Each repetition of `computetest` allocates its two buffers
  anew. The executor that keeps them was measured in the studio instead (Parallel-12,
  `xenolith-studio/tests/gpu`, `gputest timings`): with staging, device and readback buffers kept per
  block and grown by powers of two, a series of frames of one size allocates nothing after the first,
  and the round trip of a block is 3.5 / 14 / 137 ms at 10³ / 10⁴ / 10⁵ branches - THE SAME on NVIDIA,
  RADV and llvmpipe, because what it pays for is the CPU's own work per branch and not the dispatch.
- The validation layer's cost. `XL_COMPUTE_VALIDATION=1` is a correctness run, not a timing run.
