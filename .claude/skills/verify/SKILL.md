---
name: verify
description: Build and check the working tree after any code change - Psion build via DOSBox, IMG hash against the previous build, DGROUP budget with per-region deltas, PC host build, and golden-frame diffs. Run it after every edit to a .c, .h, .a, .pr or map file, before reporting the change done.
---

# verify

Run the one-shot verification and read the result:

```bash
.\tools\verify.bat
```

It takes about 15 seconds and prints one line per rung:

| Rung | OK means | FAIL means |
| --- | --- | --- |
| `psion build` | `tsc` compiled and linked; warnings are listed under it | `Error` lines from `build.log` are listed - fix them first, everything after is skipped or stale |
| `image` | always OK; says whether `PSION3D.IMG` is **unchanged** (a refactor is byte-safe) or **CHANGED** from the previous build | - |
| `dgroup` | near data within the 48 KB limit; the delta against the previous build is shown, and if anything moved the full `memcheck` breakdown follows, naming the region | over the limit - move the data far or shrink it |
| `pc build` | CMake built `pc\build\psion3d_pc.exe` | compiler errors are listed |
| `frames` | each view in `golden\views.txt` rendered identically to `golden\<name>.png` | pixel count and bounding box of the difference; the rendered frame is in `.verify\frames\<name>.png` - open it and the golden side by side |

Exit 0 is pass, 1 is a failure, 3 means a verify is already running (only one can, the DOSBox mount is pinned to this directory) - wait and rerun rather than working around it.

## Reading the two "expected" outcomes

- A **refactor** must show `image ... unchanged` and every frame matching. If the image changed, the refactor changed code - find out why before calling it a refactor. (TopSpeed codegen is sensitive: `int` vs `short` and unused includes both move the image.)
- A **feature or rendering change** shows `image ... CHANGED` and, if it touches anything visible, frame failures. Look at the rendered frames. If they show what the change intended, accept them:

```bash
.\tools\verify.bat -UpdateGolden
```

Then commit the updated `golden\*.png` with the change. Never update goldens to silence a failure you have not looked at.

## Options

- `-SkipPsion` - PC build and frames only; no DOSBox. The `dgroup` rung then reads the existing map with no baseline.
- `-SkipPc` - Psion build, hash and DGROUP only. Frames still run against the existing PC exe, so they are stale if a portable module changed.
- `-Limit <bytes>` - DGROUP limit, default 49152.

## Adding a view

Append a line to `golden\views.txt`: `<name> <psion3d_pc arguments>`. `--pos` and `--angle` take the Q8 numbers the PC HUD prints, so walk the window build to the view, copy the readout, then `-UpdateGolden`.

## Trap

Make and `tsc /m` both go by mtime. Reverting a file with a copy that preserves the original timestamp (`Copy-Item` does) leaves the objects newer than the source and nothing rebuilds; the frames then test stale code. `git checkout` and editors write a fresh mtime and are fine.
