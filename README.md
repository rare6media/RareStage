# RareStage 0.7.0

RareStage is a native Windows, GPU-rendered, real-time audio visualizer. It captures the selected Windows playback device through WASAPI loopback and renders procedural Rare-themed visuals through DirectX 11 shaders.

## Current build

- Native Win32 + DirectX 11; no browser or Electron runtime
- WASAPI loopback capture from the active playback device
- 1024-point FFT with adaptive bass, mid, high, peak, beat, spectral-flux and tempo analysis
- Audio capture and rendering run independently
- Nine authored festival scenes: Rare Monolith, Crystal Bloom, Celestial Horizon, Cyber Shrine, Vector Rush, Liquid Core, Signal Break, Hologram Garden and Rare Ascension
- Dimensional signed-distance hero forms composited with restrained 2D motion systems
- Half-float HDR scene rendering, bloom extraction, filmic tone mapping and a sharpened final composite
- VSync-locked 60 FPS by default
- Borderless fullscreen and live output-device cycling
- Automatic scene direction or manual scene selection
- Direct2D-composited Show Control that remains visible over the flip-model stage surface
- Mouse, wheel and keyboard operation with live bass/mid/high/beat meters
- Four named show presets: Clean Open, Melodic Bloom, Rare Drop and Glitch Finale
- Audio-directed automatic sequencing or deterministic show-timeline mode
- Performance, Balanced, Native HD and Ultra 4K adaptive render profiles
- VSync display output or unlocked high-refresh/120 FPS output
- Clean Spout2 sender for OBS, RareOBS and Resolume; Show Control is excluded from the shared texture
- Four Rare palettes, configurable logo scale, scene timing and transition timing
- Smooth GPU scene wipes, intensity-aware automatic sequencing and saved settings
- Multi-monitor stage output selection
- GPU shader workload with no per-frame asset uploads

## Build on Windows

Install Visual Studio 2022 with **Desktop development with C++** and CMake support. Then open a Developer PowerShell in this folder:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Run `build\\Release\\RareStage.exe`.

## Controls

| Key | Action |
|---|---|
| `1`–`9` | Select a visual scene |
| `0` or `A` | Toggle automatic scene direction |
| `D` | Cycle through Windows playback devices |
| `M` | Move stage output to the next monitor |
| `P` | Cycle Rare color palettes |
| `Tab` | Show or hide Show Control |
| Mouse click / wheel | Select and adjust a Show Control row |
| `Up` / `Down` | Select a Show Control setting |
| `Left` / `Right` | Adjust the selected setting |
| `F1`–`F4` | Load a named show preset |
| `Shift+F1`–`Shift+F4` | Save the current show into that preset slot |
| `T` | Toggle audio-directed sequencing / show timeline |
| `V` | Toggle display VSync / unlocked high-refresh output |
| `S` | Toggle the clean Spout2 sender |
| `F11` or `Alt+Enter` | Toggle borderless fullscreen |
| `Space` | Freeze/unfreeze automatic scene changes |
| `Esc` | Leave fullscreen, or quit when windowed |

Show Control reports the active scene, preset, output device and tuned bass/mid/high/beat values. Click the cyan minus/plus controls or place the pointer over a row and use the wheel. Settings and preset slots are saved in `%LOCALAPPDATA%\\RareStage\\settings.ini`. RareStage continues animating gracefully when audio is silent.

## Spout2 stage output

RareStage publishes `RareStage Clean` as a DirectX 11 Spout sender. The sender receives the final tone-mapped frame before Show Control is composited, so operator controls never appear in OBS, RareOBS or Resolume. Disable it with `S` or the `SPOUT OUTPUT` row. The official Spout2 SDK is linked statically by CMake; no extra DLL is required beside RareStage.

## Performance design

The renderer uses three GPU passes: an HDR festival-scene pass, half-resolution bloom extraction, and a filmic final composite. Dimensional forms are rendered procedurally without streaming geometry or uploading assets every frame. Audio is analyzed on a dedicated thread, and only a compact constant buffer is copied to the GPU. Performance profiles scale the HDR workload from 62% to 125% supersampling while the final output remains native-resolution and crisp.

## Next production milestones

- MIDI and OSC control mapping plus a visual show-cue editor
- User shader packs and import/exportable preset bundles
- Signed installer, crash recovery and in-app GPU diagnostics
- NDI output and synchronized multi-machine stage playback

## Third-party software

RareStage includes Spout2 under its BSD 2-Clause license. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
