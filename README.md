# Gecode - Generic Constraint Development Environment

![Gecode](images/gecode-logo-100.png "Gecode")

Gecode is an open source C++ toolkit for developing
constraint-based systems and applications. Gecode provides a
constraint solver with state-of-the-art performance while being
modular and extensible.

[master](https://github.com/Gecode/gecode/tree/master):
[![Build Status master](https://api.travis-ci.org/Gecode/gecode.svg?branch=master)](https://travis-ci.org/Gecode/gecode)

[develop](https://github.com/Gecode/gecode/tree/develop):
[![Build Status develop](https://api.travis-ci.org/Gecode/gecode.svg?branch=develop)](https://travis-ci.org/Gecode/gecode)

## Getting All the Info You Need...

You can find lots of information on
[Gecode's webpages](https://gecode.github.io),
including how to download, compile, install, and use it.

In particular,
Gecode comes with
[extensive tutorial and reference documentation](https://gecode.github.io/documentation.html).

## CMake Build Options

CMake now exposes options aligned with the Autoconf build switches.
The minimum required CMake version is 3.21.
Qt discovery uses CMake packages (Qt6 or Qt5).

| Configure switch | CMake option | Default |
|---|---|---|
| `--enable-shared` | `GECODE_BUILD_SHARED` | `ON` |
| `--enable-static` | `GECODE_BUILD_STATIC` | `OFF` |
| `--enable-thread` | `GECODE_ENABLE_THREAD` | `ON` |
| `--enable-qt` | `GECODE_ENABLE_QT` | `ON` |
| `--enable-gist` | `GECODE_ENABLE_GIST` | `ON` |
| `--enable-cpprofiler` | `GECODE_ENABLE_CPPROFILER` | `ON` |
| `--enable-cbs` | `GECODE_ENABLE_CBS` | `OFF` |
| `--enable-examples` | `GECODE_ENABLE_EXAMPLES` | `ON` |
| `--enable-search` | `GECODE_ENABLE_SEARCH` | `ON` |
| `--enable-int-vars` | `GECODE_ENABLE_INT_VARS` | `ON` |
| `--enable-set-vars` | `GECODE_ENABLE_SET_VARS` | `ON` |
| `--enable-float-vars` | `GECODE_ENABLE_FLOAT_VARS` | `ON` |
| `--enable-minimodel` | `GECODE_ENABLE_MINIMODEL` | `ON` |
| `--enable-driver` | `GECODE_ENABLE_DRIVER` | `ON` |
| `--enable-flatzinc` | `GECODE_ENABLE_FLATZINC` | `ON` |

Additional parity-oriented options are available for advanced features,
including MPFR, allocator/audit toggles, visibility, and freelist sizes.
By default, CMake uses checked-in `gecode/kernel/var-type.hpp` and
`gecode/kernel/var-imp.hpp`; regeneration is opt-in via
`-DGECODE_REGENERATE_VARIMP=ON`.

Compatibility aliases are still accepted temporarily:
`ENABLE_THREADS`, `ENABLE_GIST`, `BUILD_EXAMPLES`, `ENABLE_CPPROFILER`.

## Download Gecode

Gecode packages (source, Apple MacOS, Microsoft Windows) can be downloaded from
[GitHub](https://github.com/Gecode/gecode/releases)
or
[Gecode's webpages](https://gecode.github.io/download.html).

## Contributing to Gecode

We happily accept smaller contributions and fixes, please provide them as pull requests against the develop branch. For larger contributions, please get in touch.

## Gecode License

Gecode is licensed under the
[MIT license](https://github.com/Gecode/gecode/blob/master/LICENSE).
