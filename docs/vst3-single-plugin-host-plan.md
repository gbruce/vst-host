# VST3 Single-Plugin Host Plan

## Goal

Build a Windows-only standalone desktop application that hosts one instrument/synth VST3, opens its native UI in a floating window, exposes a searchable generic parameter panel, supports live preview and offline WAV rendering, and accepts note input from an on-screen keyboard, external MIDI devices, and simple MIDI clip import.

## Product Scope

### In scope

- Windows 10/11 x64
- Standalone desktop app only
- One hosted VST3 instrument/synth at a time
- Native plugin UI embedding
- Generic searchable parameter browser/editor
- Live audio preview
- Offline WAV rendering
- On-screen MIDI keyboard
- External MIDI input
- Simple MIDI file or clip import
- Save/load app session and plugin state

### Out of scope for v1

- Effect plugins
- Multiple hosted plugins
- Effects chains
- Automation lanes
- Full DAW transport and sequencing
- AU, VST2, CLAP, or cross-platform support
- Sandboxed subprocess hosting

## Technical Direction

- **Language:** C++20
- **Framework:** JUCE with VST3 hosting enabled
- **Build system:** CMake
- **Toolchain:** Visual Studio 2022 + MSVC
- **Audio backends:** WASAPI or ASIO
- **Packaging:** Standalone `.exe`

## Architecture Summary

Use a narrow single-plugin audio graph:

`On-screen keyboard / external MIDI / imported MIDI -> hosted VST3 synth -> stereo audio output`

Core responsibilities:

1. Load and instantiate one VST3 synth from a file path.
2. Initialize audio and MIDI devices.
3. Open the plugin's native editor in a floating window.
4. Enumerate plugin parameters and expose them in a searchable host-owned panel.
5. Route MIDI from UI keyboard, external devices, and imported MIDI clips.
6. Support both realtime playback and offline rendering.
7. Save and restore session state, including plugin state chunks.

## Main Workstreams

### 1. App shell and device management

- Create JUCE standalone app scaffold
- Set up audio device selection and persistence
- Set up MIDI input enumeration and selection

### 2. VST3 loading and lifecycle

- Load one VST3 from a selected file path
- Create and manage the plugin instance
- Handle plugin load/init/open/close failure states clearly

### 3. UI hosting and parameter control

- Open the plugin's native editor in a floating window
- Enumerate parameters with name, range, and current value
- Build a searchable generic parameter panel
- Support host-driven parameter changes safely

### 4. MIDI input and playback

- Add on-screen keyboard
- Forward external MIDI controller input
- Import a simple MIDI file or clip
- Trigger notes for live preview

### 5. Offline rendering

- Render hosted synth output to WAV from imported MIDI or a fixed note pattern
- Keep render flow separate enough to avoid UI/audio contention
- Validate sample rate, buffer size, and output format choices

### 6. Persistence

- Save/load selected plugin path
- Save/load plugin state chunk
- Save/load parameter snapshot and session settings
- Save/load MIDI clip reference and audio settings

## Key Risks

- Native editor embedding compatibility across plugins
- Parameter enumeration quirks and inconsistent metadata
- Plugins that behave differently offline vs realtime
- State restore order for plugins with fragile initialization
- Avoiding blocking or unsafe work on the audio thread

## Recommended Milestones

1. App window, audio device manager, and MIDI device setup **(done)**
2. Load one VST3 synth and produce live audio **(done)**
3. Open plugin editor in a floating window and expose generic parameter list **(done)**
4. Add on-screen keyboard and external MIDI input **(done)**
5. Add MIDI import and offline WAV render
6. Add session save/load and polish error handling

## Progress Notes

- The standalone JUCE host can load one VST3 instrument at a time and route it to the live audio output.
- Audio device settings and enabled MIDI inputs persist across launches.
- The hosted plugin's native UI opens in a separate floating editor window, while the main window keeps parameter search and host controls visible.
- The host now includes an on-screen MIDI keyboard, channel/base-octave controls, and an All Notes Off action for live preview.
- Enabled external MIDI devices continue feeding the plugin directly and now mirror note activity onto the host keyboard display.

## Success Criteria

- User can load one VST3 synth and hear it in realtime
- User can see and use the plugin's own UI
- User can search, inspect, and modify plugin parameters from the host UI
- User can play notes from an on-screen keyboard or external MIDI device
- User can import simple MIDI and export rendered WAV audio
- User can close and reopen a session without losing essential plugin state
