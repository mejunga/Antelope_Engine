# Antelope Engine

A real-time 3D game engine written in C++20 on top of Vulkan, built as a graduation project.

> **Note:** This is an abandoned, incomplete version of the project. It was built under tight time constraints as a learning exercise and carries many architectural and implementation flaws as a result. Development has stopped in favor of an upcoming rewrite that will be built with a cleaner foundation and more deliberate design. This repository will remain as a personal test board and reference.

## What it does

- Vulkan renderer with PBR materials, shadow mapping, and a post-processing pipeline
- Entity Component System via [EnTT](https://github.com/skypjack/entt)
- Rigid body physics via [JoltPhysics](https://github.com/jrouwe/JoltPhysics)
- Skeletal animation with state-machine graphs
- Spatialized audio via [miniaudio](https://miniaud.io)
- Native C++ hot-reload scripting (runtime MSVC compilation)
- Job system via [enkiTS](https://github.com/dougbinks/enkiTS) with [rpmalloc](https://github.com/mjansson/rpmalloc)
- ImGui-based editor with scene, hierarchy, properties, animator, and console panels
- YAML scene serialization

## Requirements

- Windows 10/11 (64-bit)
- Visual Studio 2022 (MSVC v143)
- [Vulkan SDK 1.3+](https://vulkan.lunarg.com/)
- [vcpkg](https://github.com/microsoft/vcpkg) — set `VCPKG_ROOT` as a system environment variable
- CMake 3.20+

## Build

Run `build.bat` from the repository root. It builds in Release mode and launches the editor with the included Sandbox project automatically.

Output executable: `build\bin\Release\AntelopeEngine.exe`

## Run manually

    build\bin\Release\AntelopeEngine.exe "path\to\project"

Pass the path to a folder containing an `.antelopeproject` file. Defaults to the working directory if omitted.
