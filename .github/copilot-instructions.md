# Copilot Instructions for Jangine

## Project Overview

- **Jangine** is a cross-platform, lightweight C++20 game engine, originally ported from C#. Simplicity is a core design goal.
- **Graphics API:** Vulkan only.
- **Build System:** Meson (`meson.build` is the authoritative source for build logic and dependencies).
- **External Dependencies:** Vulkan, Eigen, Freetype, GLFW, Harfbuzz, nlohmann/json, TinyXML2, VMA, WebP. All are managed via the `external/` directory and declared in `meson.build`.

## Architecture

- **Core Engine:** `Jangine/Core.cpp` and `Jangine/Core.hpp` manage engine lifecycle, logging, and subsystem orchestration.
- **Graphics:** The `Jangine/Gfx/` directory contains 2D/3D rendering (`Overlay.cpp`, `Core3D.cpp`), shader management (`ShaderProgram.hpp`), and presentation (`Presenter.cpp`).
- **GUI:** `Jangine/Gui/Core.cpp` integrates with GLFW for windowing and input.
- **IO:** Asset loading (GLTF, WebP) is handled in `Jangine/IO/`.
- **Tools:** Custom compilers for fonts and shaders are in `Jangine/Tools/` and invoked via Meson custom targets to generate headers (`BuiltInFonts.hpp`, `BuiltInShaders.hpp`).

## Developer Workflows

- **Build:** Use the VS Code task `"Build"` or run `meson compile -C build` from the workspace root.
- **Shader/Font Compilation:** Meson custom targets automatically invoke the compilers in `Jangine/Tools/` to generate required headers before building the main library.

## Conventions & Patterns

- **Modern C++:** Use C++20 features and idioms throughout. Prefer `std::span`, `std::format`, and smart pointers.
- **Logging:** Use the `Jangine::Logging::Logger` class. Loggers are retrieved via `Core::GetLogger("Subsystem")` and support structured logging (`Func`, `Error`, `Debug`).
- **Platform Detection:** See `Shared.hpp` for macros (`JANGINE_PLATFORM_WINDOWS`, etc.).
- **DLL Export:** Use `JANGINE_API` for public symbols (see `Shared.hpp`).
- **Generated Files:** Ignore or exclude files guarded by `JANGINE_INTELLISENSE_IGNORE_GENERATED_FILES` in code navigation.
- **Shader Patterns:** All built-in shaders are HLSL, compiled to SPIR-V via DXC. See `Jangine/Gfx/Shaders/` for examples.
- **Threading:** Use `ThreadPool.hpp` and `CancellationToken` for background work.

## Integration Points

- **Custom Asset Compilers:** Font and shader compilers are invoked by Meson, not manually. See `FontCompiler.cpp` and `ShaderCompiler.cpp` for argument conventions.
- **GLFW:** Used for window/input management in GUI (`Gui/Core.cpp`).
- **Vulkan:** All graphics code is Vulkan-centric; see `Gfx/Core.cpp` for initialization and error handling patterns.

## Key Files & Directories

- `meson.build` — Build logic, dependencies, and custom targets.
- `Jangine/Core.cpp`, `Jangine/Core.hpp` — Engine entry point and orchestration.
- `Jangine/Gfx/` — Graphics subsystem.
- `Jangine/Gui/` — GUI and window/input management.
- `Jangine/Tools/` — Asset compilers.
- `Jangine/Logging/` — Logging infrastructure.
- `Jangine/IO/` — Asset loading.