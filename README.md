# VST Host

Starter project for a Windows-only standalone JUCE application that will host a single VST3 instrument/synth.

## Current status

The repo currently contains:

- a CMake-based JUCE app scaffold
- JUCE dependency setup through `FetchContent`
- a standalone desktop window
- persisted audio/MIDI device management UI
- file-based VST3 instrument loading
- plugin scan/instantiate validation with clear load failure states
- floating native plugin editor window when available
- searchable host-side parameter panel with live parameter control

The next milestone is MIDI input and playback.

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 with Desktop C++ tools
- CMake 3.28+
- Internet access during the first configure so CMake can fetch JUCE

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

## Run

The executable will be generated under the JUCE artefacts directory, for example:

```text
build\vst_host_artefacts\Debug\VST Host.exe
```

## Current loader behavior

- `Load VST3...` opens a file chooser rooted at the standard Windows VST3 directory
- the host scans the selected module for available plugin types
- v1 accepts **instrument/synth VST3s only**
- effect-only modules are rejected with an explicit message
- a successfully loaded plugin is attached to the host audio/MIDI lifecycle and summarized in the UI

## Current editor + parameter behavior

- when a loaded plugin exposes a native editor, the app opens it in a separate floating window
- the editor window can be closed and reopened from the main host with **Show Editor**
- when a plugin has no native editor, the app keeps working through the host-side parameter panel only
- the parameter panel shows host-exposed parameters with:
  - search by name, parameter ID, category, or unit
  - current value text
  - direct slider-based host control
  - live updates as parameter values change

