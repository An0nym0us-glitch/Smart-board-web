# ClassBoard in the browser (web version)

The web version is the ClassBoard application itself, compiled to WebAssembly with
[Qt for WebAssembly](https://doc.qt.io/qt-5/wasm.html). It uses the same source code as the
desktop app (`src/`), so the board, tools, instruments, equations, calculator, graphs, pages,
templates, undo/redo and exports behave the same. It runs in any current browser (Chrome, Edge,
Firefox, Safari) on computers, interactive displays and tablets, and nothing has to be installed.

**Open it:** <https://an0nym0us-glitch.github.io/Smart-board-web/>

## Saving and opening lessons

A browser cannot write into your folders directly, so files work like this:

| Action | Desktop app | Web version |
|---|---|---|
| **Save** (Ctrl+S) | Writes the `.classboard` file | Keeps the lesson in this browser **and** downloads the `.classboard` file |
| **Save as** (Ctrl+Shift+S) | File dialog | Asks for a name, then saves as above |
| **Open** (Ctrl+O) | File dialog | The browser's file picker; the lesson is also kept in this browser |
| **Recent lessons** | Recently used files | Lessons kept in this browser (open them without a download) |
| **Autosave / recovery** | Recovery copy after a crash | Recovery copy kept in this browser: if you close or reload the tab with unsaved work, ClassBoard offers to restore it next time |
| **Export PDF / PowerPoint / PNG** | File dialog | Downloaded |
| **Import images / PDF / lessons** | File dialog or drag and drop | The browser's file picker, or drag files onto the page |

`.classboard` files are the same in both versions: a lesson saved in the browser opens in the
desktop app and the other way round.

Settings, custom templates, the lessons kept in the browser and the autosave copies are stored in
the browser's IndexedDB for this site. Clearing the browser's site data removes them, so keep the
downloaded `.classboard` files of lessons you need.

## Differences from the desktop app

* **PDF import** renders pages with [pdf.js](https://mozilla.github.io/pdf.js/) in the browser.
* **PowerPoint import** needs an office suite and is not available in the browser. Save the
  presentation as PDF and import the PDF instead.
* **Full screen** uses the browser's full screen (the button in the ribbon or *View → Full
  screen*; press Esc to leave it).
* Only one image can be picked at a time in *Insert → Images* (drag several images onto the page
  to insert them together).
* Recogniser plug-ins (shared libraries) cannot be loaded in a browser; the built-in Magic
  Equation Maker works.

## Building the web version

The GitHub workflow [`.github/workflows/web.yml`](../.github/workflows/web.yml) builds the web
version on every push and publishes it on GitHub Pages from the default branch. To enable
publishing once: *Settings → Pages → Build and deployment → Source: GitHub Actions*.

To build it yourself you need [Emscripten](https://emscripten.org) **1.39.8** (the version Qt
5.15.2 for WebAssembly is made with) and Qt 5.15.2 for WebAssembly (`wasm_32`, for example with
`aqt install-qt linux desktop 5.15.2 wasm_32`):

```bash
source ~/emsdk/emsdk_env.sh                 # after: ./emsdk install 1.39.8 && ./emsdk activate 1.39.8
mkdir build-web && cd build-web
~/Qt/5.15.2/wasm_32/bin/qmake ../web/classboard-web.pro
make -j4
cd ..
web/package.sh build-web site               # adds the page, the loader and pdf.js (needs npm)
python3 -m http.server --directory site 8000
```

Then open <http://localhost:8000>. WebAssembly pages must be served over HTTP(S); opening
`index.html` from disk does not work.

### How it fits together

* `web/classboard-web.pro` builds every file in `src/` (qmake is the build system Qt 5.15
  supports for WebAssembly; the desktop app keeps using CMake).
* `src/platform/WebPlatform.*` is the small layer that replaces file dialogs, folders and
  external programs with their browser equivalents. Everything else is shared; differences are
  marked with `Q_OS_WASM` in the code.
* `src/core/Background.h` runs saving, autosave, export and recognition on a worker thread in the
  desktop app and on the interface thread in the browser (Qt 5.15 for WebAssembly has no threads).
* `web/index.html` is the page, `web/classboard-web.js` provides storage (IndexedDB), full screen,
  pdf.js and drag and drop.
