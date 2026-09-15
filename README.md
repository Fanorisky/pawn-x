# pawn-x

Experiments and improvements to the [Pawn compiler](https://github.com/openmultiplayer/compiler)
(open.mp / SA-MP). Goal: implement natively in the compiler the language
capabilities that [YSI Includes](https://github.com/Y-Less/YSI-Includes)
currently patch from the script side (timers, inline functions, iterators,
hooks, varargs), so scripts no longer need those patches.

## Layout

| Path | Contents |
|---|---|
| `compiler/` | Vendored Pawn Community Compiler source (C) |
| `ysi-5/` | Vendored YSI 5 includes (reference for what to replace) |
| `openmp/` | open.mp server package (downloaded, gitignored) |
| `experiments/` | Experiment scripts and results |
| `docs/` | Standards, notes, experiment reports |

See `docs/CODING_STANDARDS.md` before contributing code.

## License notice

The vendored compiler is Copyright (c) ITB CompuPhase 1997-2006, modified by
the community — see `compiler/license.txt`. YSI is MPL 1.1 — see `CREDITS.md`.
