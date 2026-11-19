# Remi 3D build simulator

> For designing the screens, use the **live preview** instead: `python3 hardware/sim/dev_server.py`, then open http://localhost:8770/preview.html. It rebuilds on every firmware save and hot-reloads (see the hardware README).

This is a 3D build guide for the Remi kitchen device. The screen in the 3D view runs Remi's real firmware UI and sounds, compiled to WebAssembly, and you can turn the 3D knob to use it.

GitHub stores these files but can't run them in place. Run the simulator on your computer as shown below, or publish it with GitHub Pages.

## Run it on your computer

1. Get the code. Either clone the branch:
   ```bash
   git clone -b hardware/remi-device https://github.com/yousssssssssef/Remi.git
   ```
   ```bash
   cd Remi
   ```
   Or on GitHub, choose **Code → Download ZIP** and unzip it.
2. Start a local web server. A server is needed because browsers won't load the WebAssembly or 3D model files from a double-clicked `file://` page:
   ```bash
   python3 -m http.server 8765 --directory hardware/sim
   ```
3. Open **http://localhost:8765** in Chrome, Safari, Edge or Firefox. It needs an internet connection, because three.js and the fonts load from a CDN.
4. Stop the server with `Ctrl+C`.

Each view has a link you can bookmark:

| View | Link |
|---|---|
| Today: ESP32-CAM bench build | http://localhost:8765/#today |
| Full build with every wire | http://localhost:8765/#bench |
| Exploded enclosure | http://localhost:8765/#exploded |
| Finished Remi | http://localhost:8765/#product |

## Using it

- **Knob:** drag the 3D knob to turn it, click it to press, and hold it for about half a second to go back. The **Left / Press / Right / Hold** buttons, the arrow keys, Space and Esc do the same.
- **Sound:** press **Sound on**. Browsers only play audio after a click.
- **Camera:** drag empty space to orbit, scroll to zoom, and right-drag to pan.
- **Build steps:** click a step to highlight its wires and zoom to that part. Click it again to show everything.
- **Parts:** click any part to see its pins and specs.
- **Replay intro:** restarts the boot animation.

The simulator uses sample kitchen data. It doesn't connect to the Remi API.

## Put it online with GitHub Pages (optional)

1. On GitHub, open the repo's **Settings → Pages**.
2. Under **Build and deployment**, choose **Deploy from a branch**.
3. Pick the branch: `hardware/remi-device`, or `version-3-ui` once the PR is merged. Set the folder to `/ (root)`, then press **Save**.
4. After a minute or two, the simulator is at:
   **https://yousssssssssef.github.io/Remi/hardware/sim/**

A Pages site is public, like the repo itself.

## Rebuild it

| What | Command |
|---|---|
| Firmware UI → `remi.wasm` (needs [wasi-sdk](https://github.com/WebAssembly/wasi-sdk/releases)) | `WASI_SDK=/path/to/wasi-sdk hardware/sim/build_wasm.sh` |
| CAD part → `models/*.glb` (needs `npm install occt-import-js`) | `node hardware/sim/step_to_glb.mjs part.step hardware/sim/models/part.glb` |

Rebuild `remi.wasm` after changing `ui.cpp`, `gfx.cpp`, `synth.cpp` or the generated assets, so the simulator matches the device.

## Files

| File | What it is |
|---|---|
| `index.html` | Page layout and controls |
| `sim.js` | 3D scene, part models, wiring, views and knob handling (three.js) |
| `remi.wasm` | The device UI and synth, built from `wasm_main.cpp` and `../firmware/Remi_Device` |
| `models/max98357a.glb`, `models/speaker-40mm.glb` | Adafruit CAD parts 3006 and 3968 (MIT), converted from STEP |
| `build_wasm.sh`, `wasm_main.cpp`, `step_to_glb.mjs` | Build scripts |
