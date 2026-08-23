# RareStage 0.1.0

RareStage is a native Windows, GPU-rendered, real-time audio visualizer. It captures the selected Windows playback device through WASAPI loopback and renders procedural Rare-themed visuals through DirectX 11 shaders.

## Current build

- Native Win32 + DirectX 11; no browser or Electron runtime
- WASAPI loopback capture from the active playback device
- 1024-point FFT with bass, mid, high, peak and stereo energy extraction
- Audio capture and rendering run independently
- Nine GPU scenes: Pulse Engine, Liquid Signal, Orbital Drive, Signal Glitch, Impact Bloom, Data Current, Prism Tunnel, Celestial Drift and Velocity Burst
- VSync-locked 60 FPS by default
- Borderless fullscreen and live output-device cycling
- Automatic scene direction or manual scene selection
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
| `0` | Enable automatic scene direction |
| `D` | Cycle through Windows playback devices |
| `F11` or `Alt+Enter` | Toggle borderless fullscreen |
| `Space` | Freeze/unfreeze automatic scene changes |
| `Esc` | Leave fullscreen, or quit when windowed |

The title bar reports the active scene, output device, live bass/mid/high/beat values and measured frame rate. RareStage continues animating gracefully when audio is silent.

## Performance design

The renderer uses a single fullscreen triangle and a procedural pixel shader. Audio is analyzed on a dedicated thread, and only a compact constant buffer is copied to the GPU per frame. This keeps draw-call count, memory traffic and CPU overhead low. VSync avoids wasteful rendering above the display refresh rate.

## Next production milestones

- In-app device/settings panel and sensitivity calibration
- Spout2 output for OBS, RareOBS and Resolume
- Beat/onset detector with tempo confidence
- Shader preset packs, transitions and show timeline
- 120 FPS and 4K adaptive-quality profiles
- Signed installer, crash recovery and GPU diagnostics
