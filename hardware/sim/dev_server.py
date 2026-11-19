"""Live preview server for the Remi device UI (like a phone simulator, for the ESP32 screen).

  python3 hardware/sim/dev_server.py          # then open http://localhost:8770/preview.html

Serves hardware/sim, watches the firmware sources, and rebuilds remi.wasm (and the generated boot/font
assets when their inputs change) whenever a file is saved. The page polls build.json and hot-swaps the
new build, replaying your inputs so you stay on the same screen. Build errors show on the page.
Python standard library only. Needs wasi-sdk (sim/build_wasm.sh says where it looks).
"""
import json, os, subprocess, sys, threading, time
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HW = Path(__file__).resolve().parents[1]
SIM = HW / 'sim'
PORT = int(os.environ.get('PORT', '8770'))
UI_SOURCES = [HW / 'firmware' / 'Remi_Device', HW / 'preview' / 'sample_backend.h', SIM / 'wasm_main.cpp']
ASSET_SOURCES = [HW / 'tools' / 'build_assets.py', HW / 'assets']
UI_EXT = {'.cpp', '.h', '.c'}

state = {'build': 0, 'ok': True, 'state': 'idle', 'time': time.time(), 'error': ''}


def write_state(**kw):
    state.update(kw)
    tmp = SIM / 'build.json.tmp'
    tmp.write_text(json.dumps(state))
    os.replace(tmp, SIM / 'build.json')


def snapshot(paths, exts=None):
    stamps = {}
    for root in paths:
        files = [root] if root.is_file() else root.rglob('*')
        for f in files:
            if f.is_file() and (exts is None or f.suffix in exts):
                stamps[str(f)] = f.stat().st_mtime_ns
    return stamps


def run(cmd, env=None):
    p = subprocess.run(cmd, cwd=HW, capture_output=True, text=True, env={**os.environ, **(env or {})})
    return p.returncode == 0, (p.stdout + p.stderr).strip()


def rebuild(assets_changed):
    write_state(state='building')
    started = time.time()
    if assets_changed:
        print('assets changed: regenerating boot frames, fonts and icons...', flush=True)
        ok, log = run([os.environ.get('PY', sys.executable), 'tools/build_assets.py'])
        if not ok:
            write_state(state='idle', ok=False, error='Asset build failed:\n' + log[-4000:])
            return
    ok, log = run(['zsh', 'sim/build_wasm.sh'], {'OUT': 'sim/remi.next.wasm'})
    if ok:
        os.replace(SIM / 'remi.next.wasm', SIM / 'remi.wasm')
        write_state(state='idle', ok=True, build=state['build'] + 1, time=time.time(), error='')
        print(f'build {state["build"]} ok in {time.time() - started:.1f}s', flush=True)
    else:
        # keep the last good remi.wasm; the page shows the compiler output
        errors = '\n'.join(l for l in log.splitlines() if 'error' in l or l.startswith(' ')) or log
        write_state(state='idle', ok=False, error=errors[-4000:])
        print('build failed:\n' + errors[-2000:], flush=True)


def watch():
    ui = snapshot(UI_SOURCES, UI_EXT)
    assets = snapshot(ASSET_SOURCES)
    rebuild(False)
    while True:
        time.sleep(0.4)
        new_ui, new_assets = snapshot(UI_SOURCES, UI_EXT), snapshot(ASSET_SOURCES)
        changed_ui = {k for k in set(ui) | set(new_ui) if ui.get(k) != new_ui.get(k)}
        changed_assets = new_assets != assets
        if changed_ui or changed_assets:
            time.sleep(0.25)  # let editors finish writing
            new_ui, new_assets = snapshot(UI_SOURCES, UI_EXT), snapshot(ASSET_SOURCES)
            names = sorted(Path(k).name for k in changed_ui)
            if changed_assets or names:
                print('changed:', ', '.join(names) or 'assets', flush=True)
            rebuild(changed_assets)
            ui, assets = snapshot(UI_SOURCES, UI_EXT), snapshot(ASSET_SOURCES)  # absorb files the build regenerated


class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()

    def log_message(self, *args):  # keep the console for build output
        pass


if __name__ == '__main__':
    threading.Thread(target=watch, daemon=True).start()
    server = ThreadingHTTPServer(('127.0.0.1', PORT), partial(Handler, directory=str(SIM)))
    print(f'Remi live preview: http://localhost:{PORT}/preview.html  (watching firmware/Remi_Device)', flush=True)
    server.serve_forever()
