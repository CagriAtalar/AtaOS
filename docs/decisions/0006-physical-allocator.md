# ADR-0006: Physical allocator strategy

- **Status:** accepted (design; implemented in M3)
- **Date:** 2026-10-13

## Context and problem

Need a frame allocator over Limine's usable regions: simple, testable, with
accounting and no allocation of its own metadata from a heap.

## Considered alternatives

1. **Bitmap allocator** — 1 bit per 4 KiB frame; trivial invariants; metadata
   size = RAM/32768 (e.g. 128 KiB for 4 GiB); O(n) worst-case scan, mitigated by a
   "next-fit" hint; easy to detect double-free.
2. **Free-list (intrusive stack of free frames)** — O(1) alloc/free; metadata lives
   in the free frames themselves (needs HHDM access and trust that frames are
   not corrupted); harder to detect double-free and to allocate contiguous runs.
3. **Buddy allocator** — fast contiguous allocation and coalescing; more
   complex; DMA/contiguity needs are not present in the 60-day plan.

## Decision

**Bitmap allocator** with a next-fit hint, a bitmap placed in the first usable
region large enough (accessed via HHDM), all frames initially *reserved*, then
usable regions cleared in the bitmap. Pure bitmap/search logic lives in
`kernel/mm/bitmap.c` so it can be host-tested; the Limine-facing glue is
separate. Contiguous allocation is a linear first-fit scan on the bitmap (rarely used).

## Rationale

Correctness and testability outweigh speed at this size; double-free detection
is a direct bit check; accounting is a counter; replacing it later behind
`pmm_alloc_frame()/pmm_free_frame()` is cheap.

## Trade-offs and consequences

Allocation latency grows with fragmentation of the hint; a full scan is
`O(frames/64)` word compares. Acceptable for ≤ a few GiB.

## Testing and verification implications

Host tests: reserved bits never allocated, exhaustion returns invalid,
double-free detected, next-fit wraps, overflow on region bounds rejected,
alloc/free sweeps leave counters unchanged. QEMU: self-test at boot.

## Conditions that could justify revisiting

Boot-time allocation measurably slow; need for large contiguous DMA buffers;
NUMA/multi-zone requirements.
