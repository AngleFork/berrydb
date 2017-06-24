# BerryDB - An Experimental Key-Value Store

[![Build Status](https://travis-ci.org/google/berrydb.svg?branch=master)](https://travis-ci.org/google/berrydb)
[![Build Status](https://ci.appveyor.com/api/projects/status/rcys8rqbjpymauuu/branch/master?svg=true)](https://ci.appveyor.com/project/pwnall/berrydb)

**This is not an official Google product.**
[LevelDB](https://github.com/google/leveldb) is a stable key-value store that is
widely used at Google.

This is an experimental implementation of a key-value store.


## Prerequisites

This project uses [CMake](https://cmake.org/) for building and testing. CMake is
available in all popular Linux distributions, as well as in
[Homebrew](https://brew.sh/).

This project uses submodules for dependency management.

```bash
git submodule update --init --recursive
```

If you're using [Atom](https://atom.io/), the following packages can help.

```bash
apm install autocomplete-clang build build-cmake clang-format docblockr \
    language-cmake linter linter-clang
```

If you don't mind more setup in return for more speed, replace
`autocomplete-clang` and `linter-clang` with `you-complete-me`. This requires
[setting up ycmd](https://github.com/Valloric/ycmd#building).

```bash
apm install autocomplete-plus build build-cmake clang-format docblockr \
    language-cmake linter you-complete-me
```


## Building

The following commands build the project.

```bash
mkdir out
cd out
cmake .. && cmake --build .
```


## Development

The following command (when executed from `out/`) (re)builds the project and
runs the tests.

```bash
cmake .. && cmake --build . && ctest --output-on-failure
```

Most third-party libraries used by this project can only be used in specific components. `CMakeLists.txt` enforces these constraints, and  [third_party/README.md](./third_party/README.md) describes the motivations
behind them.
