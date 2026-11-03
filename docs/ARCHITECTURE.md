# ClassBoard architecture

ClassBoard is organised in small modules that each have one job. Only `app/` knows about all
of them.

```text
            ┌────────────────────────── app/ ───────────────────────────┐
            │ MainWindow · LessonController · ExportController ·        │
            │ PopoverController · AppSettings · AppServices             │
            └──────┬───────────────┬──────────────┬──────────────┬──────┘
                   │               │              │              │
               ui/ (ribbon,    canvas/ ──── tools/ ────── geometry/ (instruments)
               popovers,         │  ▲          │
               theme)            │  │ input/   │
                   │             ▼  │          ▼
                   └──────────► document/ ◄── math/ · graph/
                                   │
                          storage/ · export/
```

## Document model (`document/`)

```text
Document ── pages: [Page]                 ── ImageStore (shared, deduplicated assets)
   │           └─ objects: [DocumentObject] (z-order)
   │                 Stroke · Shape · Text · Equation · Image · Graph · Table
   │                 Geometry (point/segment/line/ray/vector) · Measurement
   └─ CommandStack (the only undo/redo history)
```

* Every object keeps its geometry in a local frame centred on the origin. A transform
  (translation + rotation) places it on the page. Resizing, rotating, control points,
  hit testing and serialisation are virtual methods on `DocumentObject`.
* All changes go through `Command`s: `AddObjects`, `RemoveObjects`, `ModifyObjects`
  (before/after clones), `ReplaceObjects` (eraser splitting), `ReorderObject`,
  `InsertPage`, `RemovePage`, `MovePage` and `ModifyPage`. Tools never keep their own
  undo stack. Interactive edits such as dragging change the object live, then record one
  command with `pushApplied`.
* Clones share large data. Stroke points use `QVector`'s implicit sharing and images live
  in the `ImageStore`, so an undo snapshot costs very little memory.
* `Document` emits fine-grained signals (`objectAdded/Removed/Changed` with bounds). The
  canvas uses them to repaint only the affected region.

## Rendering (`canvas/`)

* `PageRenderer` draws a page (template background plus objects). The canvas, thumbnails
  and every exporter use it, so what you see is what you export.
* `CanvasWidget` keeps a cached pixmap of the committed content with a dirty region.
  Changes repaint only their bounds. Panning scrolls the cache and renders only the newly
  exposed strips. Pinch and wheel zoom show a scaled preview of the cache and re-render
  once zooming settles.
* Live content is drawn as an overlay on top of the cache: ink in progress, tool previews,
  selection handles and instruments.
* `MagicHighlightLayer` is a temporary effect layer owned by the canvas (tools reach it via
  `ToolHost::magicHighlights()`). `PenTool` hands finished Magic Highlighter strokes to it
  instead of pushing an `AddObjectsCommand`, so they are never document objects: not
  serialised, exported, counted or undoable. Each highlight stays for
  `MAGIC_HIGHLIGHTER_DURATION_MS`, fades over `MAGIC_HIGHLIGHTER_FADE_MS` (both in
  `MagicHighlightLayer.h`) and is dropped; its timer runs only while highlights exist. Page
  changes and new lessons clear the layer.
* Templates are procedural and cover an infinite canvas. Grids fade out when they would be
  too dense on screen. The nominal 16:9 page frame is only used for export and "fit".

## Input (`input/`)

```text
QMouseEvent / QTabletEvent / QTouchEvent
        │
   InputManager ── pointer events (id, device, phase, pressure) ──► instruments / ToolController
        │
        └──────── gesture events (pan/zoom/rotate, palm erase) ────► canvas / instrument / tool
```

* Mouse events that the OS or Qt synthesises from touch and pen are ignored, because those
  devices are handled natively.
* **Ten-point touch.** Every contact is tracked by its own touch id and gets one role: *write*
  (its own pointer, independent of all others), *gesture* (pan/zoom), *palm* (wipe) or
  *ignored*. A new finger writes unless:
  * it completes a tight group of three (wipe, see below);
  * a pan/zoom is running: a finger near the gesture centre joins it, others are ignored
    until lifted (writing while the view moves would bend the line);
  * it pairs with a writing finger whose stroke is still young (moved less than 12 mm, or
    started less than 350 ms ago and shorter than 30 mm): either within 160 mm (one hand),
    or within 450 mm and landing at most 200 ms after it (one finger of each hand). Young
    strokes are cancelled and the two fingers pan/zoom. Any established stroke blocks this,
    so zoom never starts while somebody is really writing and a real line is never
    interrupted.
  All distances are physical millimetres converted with the screen's logical pixels per
  millimetre (`CanvasWidget::pixelsPerMm`), so they hold at every Windows display scaling.
  Fingers left over from a gesture never start drawing. The "every finger draws" setting
  (`setMultiUserTouch`) turns pan/zoom off.
* **Pen priority.** While the stylus touches the board or hovers over it (and for 400 ms
  afterwards) new touches are ignored, so the writing hand neither draws nor palm-erases. A
  young finger stroke is cancelled when the pen lands.
* **Wipe eraser.** Three fingers landing within 450 ms and packed tightly together (every
  centre-to-centre distance and the group's bounding box within 40 mm), or one very large
  contact, start the *wipe*: one grouped gesture driven only by the fingers of the group, that
  never produces pointer events and becomes one undo step. All thresholds are in
  `input/PalmGesture.h`. The eraser splits ink under the wipe and removes other objects it
  hits (their own `hitTest`), except pictures and imported pages. Other people keep writing
  during a wipe; fingers within 160 mm of it belong to the wiping hand and are ignored. The
  group's just-started strokes are cancelled.
* Every pointer belongs to the tool that received its *down* event, so switching tools in the
  middle of a stroke is safe. The stylus eraser end is routed to the eraser automatically.

## Tools (`tools/`) and instruments (`geometry/`)

Tools implement a small interface: pointer down/move/up/cancel, hover, overlay painting,
keys and gestures. They work through the `ToolHost` interface, never directly with widgets.
The tools are pen, eraser, select, shape, text (inline editor), measure, construct and graph.

Instruments (ruler, protractor, set square, compass) are overlays. They are not document
content, just like a physical ruler lying on the board. Each one exposes handles and *edge
constraints*: ink started near an edge is projected onto that straight line or arc. The
compass and the protractor's stamp button produce document objects.

## Mathematics (`math/`, `graph/`)

* `CoordinateSystem` maps page coordinates to mathematical units. One global system serves
  the coordinate-plane template and all measurements. A `GraphObject` provides its own
  system, and `resolveCoordinateSystem` picks the right one for a set of points. As a
  result, measurements, points and vectors placed inside a graph use graph units.
* `math::Expression` compiles expressions such as `a·sin(bx)+c`, `2x²`, `|x|` and `√x` into
  a stack program for fast plotting. It handles implicit multiplication and splits
  multi-letter parameters. Its `Options` switch trigonometry to degrees (exact at 30°, 45°,
  90° …), and it also reads postfix `!` (factorial) and `%`.
* `math::Calculator` (owned by `MainWindow`, shared through `AppServices`) evaluates what is
  typed in the Calculator panel with that engine. It keeps Ans and the recent calculations,
  formats results like a school calculator and turns a calculation into LaTeX for "Put on
  board" (an ordinary `EquationObject`, one undo step).
* `mathtype::MathTypesetter` parses a LaTeX subset into a box tree (rows, fractions,
  radicals, scripts, limits, stretchy delimiters, matrices, accents). It paints with
  QPainter text and paths, so formulas stay vector in PDF export.

* **Scale and precision.** `MeasureScale` (in the lesson's `CoordinateSystem`) converts board
  lengths to real units ("10 cm = 1 km"); graph axes are unitless and never scaled.
  `geometry/Precision` holds the exact-value operations (length, direction, angle,
  rise/run/slope, polar vectors, shape metrics) as pure functions on page points; the property
  panel applies them as ordinary undoable commands. Everything works in document units, so
  zoom, DPI and window size never change a value.
* **Magic Equation Maker.** `ai/MathInkRecognizer` is a built-in offline recogniser: strokes
  are grouped into symbols, fraction bars and equals signs are found geometrically, other
  symbols are classified with a $P point-cloud matcher, and a layout step detects powers and
  fractions. It rejects uncertain input. Recognition runs on a worker thread and only produces
  a preview; `tools/MagicEquation::acceptEquation` replaces the strokes in one undoable step
  after the teacher accepts. Plug-in recognisers (`EquationRecognizer`) take precedence.

## UI (`ui/`)

* `Theme` is data driven (`resources/themes/dark.json`). Colours and metrics are named roles,
  and every size goes through `dp()` so the interface scales for 1080p, 1440p and 4K. The
  scale is chosen automatically and can be adjusted in Settings.
* `Popover` / `PopoverHost` form the single popover system: child widgets (never separate
  windows) with a rounded bubble, a soft shadow, a notch pointing at the anchor, a fade/slide
  animation, a back stack for nested panels, outside-tap dismissal, edge-aware placement and
  touch scrolling. Every panel in `ui/popovers/` is plain content placed inside it.
* Confirmations (`ConfirmOverlay`) and notifications (`Toast`) are drawn inside the window.
  Native dialogs are used only to pick files.

## Storage and export

* `.classboard` files use a CRC-checked container: compressed JSON document plus raw image
  assets. JSON documents carry a `formatVersion`, and `ProjectSerializer::migrate` upgrades
  older documents step by step. See [FILE_FORMAT.md](FILE_FORMAT.md).
* Saves are atomic (`QSaveFile`), and serialising and writing run on a worker thread.
* Autosave writes a recovery copy on a worker thread while there are unsaved changes. Each
  running session holds a lock file. On start-up, recovery copies whose lock is stale are
  offered for restore.
* Exports run on worker threads from immutable `DocumentSnapshot`s, with progress shown in
  the toast. They render the complete logical page from document coordinates (never the
  view), so zoom and scrolling have no influence. PDF output is vector with real text and each
  page has its own size (A4 stays A4). PPTX output is one high-resolution picture per slide;
  the slide follows the first page's aspect ratio and other pages are fitted whole.
  PowerPoint cannot represent ink or formulas natively, so this keeps them looking exactly
  right.
* Imports: `PdfImporter` renders PDF pages (Qt PDF or Poppler), `PresentationImporter`
  converts .ppt/.pptx to PDF with PowerPoint or LibreOffice first. `insertImportedPages`
  gives every page the source's exact aspect ratio (PDFs keep their physical size) with the
  rendered page as an image filling it, as one undoable step.

## Memory and performance

* Images are decoded at most once, limited to 4096 px per side, and never re-encoded when
  saving. Down-scaled levels of detail are created on demand for the canvas.
* The render cache is one screen-sized pixmap (about 33 MB at 4K).
* The command history is limited to 300 steps, and clones share data.
* Heavy work (saving, autosave, export, PDF conversion) never runs on the GUI thread.
