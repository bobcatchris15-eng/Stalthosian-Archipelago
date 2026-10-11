# Independent lifecycle review

Reviewed the isolated Jules lifecycle draft and kept changes to the three assigned files. The implementation now rejects invalid species during construction and invalid physical as well as calendar deltas. Transition computation happens in local state with capacity reserved before age/stage are committed, preserving state if validation, overflow, or allocation fails.

Tests now check all five profile keys and draft durations, Release-active explicit checks, physical-seconds preservation, invalid inputs, invalid profile enum, restoration consistency, big jumps, terminal death, and same-instance state preservation after overflow. The doc now describes invalid physical time and no-change-on-error accurately. Reproduction, parenting, and needs remain unfinished. Host/CMake integration remains outstanding.

Validation: MSVC C++17 optimized Release build (`/O2 /DNDEBUG`) compiled successfully; output: `6/6 tests passed`.