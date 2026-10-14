# BTAP

BTAP is an LLVM pass built to experiment with runtime bounds checking in C++.
It currently handles a small, predictable case: reads and writes through
fixed-size integer arrays allocated on the stack.

For example, given:

```cpp
int values[8] = {};
values[index] = 42;
return values[index];
```

the pass adds a check before the load or store. A negative index or an index of
`8` or greater calls the BTAP runtime, which prints what failed and exits with
status `86`.

The flow is:

```text
C++ → Clang IR → BTAP pass → optimized IR → native executable
```

The pass runs before optimization, while the stack-array access is still easy
to identify in LLVM IR. The runtime is kept separate from the pass so the
instrumented program only links in a small reporting function.

## Building

This was tested on Arch Linux with LLVM and Clang 23.1.1.

```bash
sudo pacman -S llvm clang cmake ninja
cmake -S . -B build -G Ninja -DLLVM_DIR="$(llvm-config --cmakedir)"
cmake --build build
```

Run the example like this:

```bash
scripts/instrument.sh examples/array_demo.cpp build/BTAPBoundsCheck.so out/array_demo
out/array_demo 7
out/array_demo -1
```

The first command prints `42`. The second reports a bounds failure and exits
with `86`.

## Tests

```bash
scripts/test.sh build/BTAPBoundsCheck.so
```

The tests run the instrumented programs at final `-O0` and `-O2`. They cover
normal accesses, negative and upper-bound failures for reads and writes, loops,
conditional accesses, unused one-past pointers, and running the pass twice.
The transformed IR is verified before and after optimization.

The current test run passes on LLVM 23.1.1.

## Limits

This is not a general memory-safety tool. BTAP does not currently track heap
allocations, globals, aliases, pointer arithmetic, casts, multidimensional
arrays, atomics, or volatile accesses. Unsupported patterns are left alone.

Relevant code lives in `passes/`, the runtime is in `runtime/`, and the example
and regression tests are in `examples/` and `tests/`.
