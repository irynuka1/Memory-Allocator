# Tests

- `snippets/` – small C programs (`test-<function>-<scenario>.c`) linked dynamically against `libosmem.so`. `test-utils.h` holds shared helpers.
- `ref/` – reference traces: the expected `os_*` calls and nested syscalls for each snippet.
- `run_tests.py` – runs each snippet under `ltrace`, normalizes addresses to labels (heap start, `mmap` regions), and diffs the result against the reference.
- `Makefile` – `check`, `check-fast`, `lint`, and per-snippet build rules.
- `.ltrace.conf` – tells `ltrace` how to print the `os_*` function signatures.

## Usage

```console
$ make check-fast                       # build and run all tests, no leak checks
$ python3 run_tests.py -v test-all      # print the test's own output
$ python3 run_tests.py -d <test>        # show diff against the reference
$ python3 run_tests.py -m <test>        # diff plus memory-leak report
```

Scenarios covered for `malloc`, `calloc` and `realloc`: preallocation, block reuse, expanding, splitting (first / middle / last / vector), coalescing, and array workloads.
