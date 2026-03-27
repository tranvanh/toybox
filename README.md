# Toybox

A C++23 utility library providing networking, threading, serialization, and callback primitives.

## Requirements

- CMake 3.22+
- C++23 compiler
- Boost (headers only)
- Ninja Multi-Config generator (when building standalone)

## Usage via FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    Toybox
    GIT_REPOSITORY https://gitlab.com/tranvanh/toybox.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Toybox)

target_link_libraries(MyTarget PRIVATE Toybox)
```

Then include headers as:

```cpp
#include <Toybox/FlatMap.h>
#include <Toybox/ThreadPool.h>
// etc.
```

## Components

| Header | Description |
|---|---|
| `Application.h` | Base application lifecycle with a managed thread pool |
| `Client.h` | Async TCP client (Boost.Asio) |
| `Server.h` | Async TCP server (Boost.Asio) |
| `NetworkComponent.h` | Shared networking primitives |
| `ThreadPool.h` | Fixed-size thread pool |
| `ThreadSafeQueue.h` | Thread-safe queue with priority support |
| `CallbackList.h` | Thread-safe list of callbacks |
| `CallbackLifetime.h` | RAII handle for managing callback lifetimes |
| `CallbackOwner.h` | Owns a set of callback registrations |
| `FlatMap.h` | Sorted flat map with serialization support |
| `Serialization.h` | `ostream` serialization for ranges, pairs, and custom types |
| `Logger.h` | Thread-safe logger with timestamps |
| `Common.h` | Shared macros and namespace definitions |

## Building Standalone

```sh
cmake -G "Ninja Multi-Config" -B build
cmake --build build --config Debug
```

To build with tests:

```sh
cmake -G "Ninja Multi-Config" -B build -DBUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build --build-config Debug
```

To build with benchmarks:

```sh
cmake -G "Ninja Multi-Config" -B build -DBUILD_BENCHMARK=ON
cmake --build build --config Release
```

## Build Configurations

| Config | Macro defined | Use |
|---|---|---|
| `Debug` | `DEBUG_BUILD` | Development |
| `Release` | `RELEASE_BUILD` | Production |
| `Assert` | `ASSERT_BUILD` | Debug with assertions |
