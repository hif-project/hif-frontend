# hif-frontend

**hif-frontend** provides `verilog2hif` and `vhdl2hif` — Flex/Bison-based parsers that translate Verilog and VHDL source into the HDL-Independent Format (HIF), the shared intermediate representation used across this toolchain.

Part of the HIF project. See https://github.com/hif-project for the
complete list of repositories and tools.

![CI](https://github.com/hif-project/hif-frontend/actions/workflows/ci.yml/badge.svg?branch=develop)

## Requirements

- Linux (only supported/tested platform)
- CMake ≥ 3.1, a C++17 compiler (GCC or Clang)
- Flex, Bison
- A build of [hif-core](https://github.com/hif-project/hif-core)

## Building

`hif-frontend` links against `hif-core` via `find_package(HIF REQUIRED)` (see `cmake/FindHIF.cmake`). If `hif-core` isn't installed system-wide, either check it out as a sibling directory (`../hif-core`, built with its own `cmake`/`make` — no install needed), or point CMake at it explicitly:

```sh
mkdir build && cd build
cmake -DHIF_DIR=/path/to/hif-core ..
make
```

## Running tests

```sh
ctest --test-dir build --output-on-failure
```

## Examples

`examples/` contains sample Verilog/VHDL inputs used for manual exercising of `verilog2hif`/`vhdl2hif`; they aren't run automatically by the build or by CTest.

## Documentation

If Doxygen is available:

```sh
make frontend_documentation
```

## License

BSD 2-Clause. See [LICENSE.md](LICENSE.md).
