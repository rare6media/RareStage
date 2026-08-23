# RareStage 0.5.0

RareStage is a native Windows, GPU-rendered, real-time audio visualizer. It captures the selected Windows playback device through WASAPI loopback and renders procedural Rare-themed visuals through DirectX 11 shaders.

## Current build

- Native Win32 + DirectX 11; no browser or Electron runtime
- WASAPI loopback capture from the active playback device
- 1024-point FFT with adaptive bass, mid, high, peak, beat, spectral-flux and tempo analysis
- Audio capture and rendering run independently
- Nine GPU scenes: Pulse Engine, Liquid Signal, Orbital Drive, Signal Glitch, Impact Bloom, Data Current, Prism Tunnel, Celestial Drift and Velocity Burst
- VSync-locked 60 FPS by default
- Borderless fullscreen and live output-device cycling
- Automatic scene direction or manual scene selection
- Native show-control HUD with four reaction profiles and independent band tuning
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
| `Up` / `Down` | Select a Show Control setting |
| `Left` / `Right` | Adjust the selected setting |
| `F11` or `Alt+Enter` | Toggle borderless fullscreen |
| `Space` | Freeze/unfreeze automatic scene changes |
| `Esc` | Leave fullscreen, or quit when windowed |

Show Control reports the active scene, output device, tuned bass/mid/high/beat values, estimated BPM, tempo confidence and output monitor. Settings are saved in `%LOCALAPPDATA%\\RareStage\\settings.ini`. RareStage continues animating gracefully when audio is silent.

## Performance design

The renderer uses a single fullscreen triangle and a procedural pixel shader. Audio is analyzed on a dedicated thread, and only a compact constant buffer is copied to the GPU per frame. This keeps draw-call count, memory traffic and CPU overhead low. VSync avoids wasteful rendering above the display refresh rate.

## Next production milestones

- Spout2 output for OBS, RareOBS and Resolume
- Mouse-driven controls, shader preset packs and show timeline
- 120 FPS and 4K adaptive-quality profiles
- Signed installer, crash recovery and GPU diagnostics
