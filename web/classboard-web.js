// Browser side of the ClassBoard web version.
//
// The application itself is the ClassBoard C++ code compiled to WebAssembly. This script gives it
// what a browser offers instead of an operating system:
//
//  * storage: the folder /classboard of the application's file system is kept in IndexedDB, so
//    settings, templates, saved lessons and the autosave copies survive closing the tab;
//  * full screen;
//  * PDF import with pdf.js;
//  * dropping files onto the page.
//
// The C++ side calls window.classboardWeb.* (src/platform/WebPlatform.cpp) and this script calls
// back through functions the application registers on Module (classboard*).

(function () {
    "use strict";

    var STORAGE_ROOT = "/classboard";
    var persistRunning = false;
    var persistAgain = false;
    var storageReady = false;
    var cancelledPdf = {};
    var pdfjsPromise = null;

    function appReady() {
        return typeof Module !== "undefined" && typeof Module.classboardFlush === "function";
    }

    // ------------------------------------------------------------------------- persistent storage

    // Runs before main(): mounts the IndexedDB folder and loads its contents first.
    function mountStorage() {
        if (typeof FS === "undefined" || typeof IDBFS === "undefined") {
            console.warn("ClassBoard: IndexedDB storage is not available; nothing will be kept.");
            return;
        }
        try {
            FS.mkdir(STORAGE_ROOT);
        } catch (e) { /* exists */ }
        FS.mount(IDBFS, {}, STORAGE_ROOT);
        addRunDependency("classboard-storage");
        FS.syncfs(true, function (err) {
            if (err)
                console.warn("ClassBoard: could not load saved data", err);
            ["/config", "/data", "/lessons"].forEach(function (dir) {
                try { FS.mkdir(STORAGE_ROOT + dir); } catch (e) { /* exists */ }
            });
            storageReady = true;
            removeRunDependency("classboard-storage");
        });
    }

    function persist() {
        if (!storageReady || typeof FS === "undefined")
            return;
        if (persistRunning) {
            persistAgain = true;
            return;
        }
        persistRunning = true;
        FS.syncfs(false, function (err) {
            persistRunning = false;
            if (err)
                console.warn("ClassBoard: could not save to browser storage", err);
            if (persistAgain) {
                persistAgain = false;
                persist();
            }
        });
    }

    function flush() {
        if (appReady()) {
            try {
                Module.classboardFlush(); // writes the autosave copy, then persists
                return;
            } catch (e) {
                console.warn(e);
            }
        }
        persist();
    }

    // Safety net for settings that Qt writes on its own schedule.
    setInterval(persist, 15000);
    document.addEventListener("visibilitychange", function () {
        if (document.visibilityState === "hidden")
            flush();
    });
    window.addEventListener("pagehide", flush);

    // Ask the browser not to evict the stored lessons under storage pressure.
    if (navigator.storage && navigator.storage.persist)
        navigator.storage.persist().catch(function () {});

    // -------------------------------------------------------------------------------- full screen

    function isFullScreen() {
        return !!(document.fullscreenElement || document.webkitFullscreenElement);
    }

    function setFullScreen(on) {
        var el = document.documentElement;
        try {
            if (on && !isFullScreen()) {
                var request = el.requestFullscreen || el.webkitRequestFullscreen;
                if (request) {
                    var r = request.call(el);
                    if (r && r.catch) r.catch(function (e) { console.warn("Full screen refused:", e); });
                }
            } else if (!on && isFullScreen()) {
                var exit = document.exitFullscreen || document.webkitExitFullscreen;
                if (exit) exit.call(document);
            }
        } catch (e) {
            console.warn("Full screen is not available:", e);
        }
    }

    function fullScreenChanged() {
        if (appReady() && Module.classboardFullScreenChanged)
            Module.classboardFullScreenChanged(isFullScreen());
    }
    document.addEventListener("fullscreenchange", fullScreenChanged);
    document.addEventListener("webkitfullscreenchange", fullScreenChanged);

    // ----------------------------------------------------------------------------------- pdf.js

    function loadPdfJs() {
        if (window.pdfjsLib)
            return Promise.resolve(window.pdfjsLib);
        if (!pdfjsPromise) {
            pdfjsPromise = new Promise(function (resolve, reject) {
                var script = document.createElement("script");
                script.src = "pdfjs/pdf.min.js";
                script.onload = function () {
                    if (!window.pdfjsLib) {
                        reject(new Error("pdf.js did not load"));
                        return;
                    }
                    window.pdfjsLib.GlobalWorkerOptions.workerSrc = "pdfjs/pdf.worker.min.js";
                    resolve(window.pdfjsLib);
                };
                script.onerror = function () {
                    pdfjsPromise = null;
                    reject(new Error("The PDF reader could not be loaded. Check the internet connection."));
                };
                document.head.appendChild(script);
            });
        }
        return pdfjsPromise;
    }

    function canvasToPng(canvas) {
        return new Promise(function (resolve, reject) {
            canvas.toBlob(function (blob) {
                if (!blob) {
                    reject(new Error("A page could not be converted to an image."));
                    return;
                }
                blob.arrayBuffer().then(function (buffer) { resolve(new Uint8Array(buffer)); }, reject);
            }, "image/png");
        });
    }

    async function renderPdf(id, bytes, dpi) {
        var MAX_SIDE = 4096; // keeps very large pages within browser canvas limits
        try {
            var pdfjs = await loadPdfJs();
            var doc = await pdfjs.getDocument({ data: bytes, isEvalSupported: false }).promise;
            for (var n = 1; n <= doc.numPages; ++n) {
                if (cancelledPdf[id])
                    break;
                var page = await doc.getPage(n);
                var points = page.getViewport({ scale: 1 });
                var scale = dpi / 72;
                scale = Math.min(scale, MAX_SIDE / Math.max(points.width, points.height));
                var viewport = page.getViewport({ scale: scale });
                var canvas = document.createElement("canvas");
                canvas.width = Math.max(1, Math.round(viewport.width));
                canvas.height = Math.max(1, Math.round(viewport.height));
                var ctx = canvas.getContext("2d");
                ctx.fillStyle = "#ffffff";
                ctx.fillRect(0, 0, canvas.width, canvas.height);
                await page.render({ canvasContext: ctx, viewport: viewport }).promise;
                var png = await canvasToPng(canvas);
                page.cleanup();
                if (!cancelledPdf[id])
                    Module.classboardPdfPage(id, png, canvas.width, canvas.height, points.width, points.height);
            }
            doc.destroy();
            if (!cancelledPdf[id])
                Module.classboardPdfDone(id, "");
        } catch (e) {
            console.warn("PDF import failed:", e);
            var message = (e && e.name === "PasswordException")
                ? "The PDF is protected with a password."
                : (e && e.message && e.message.indexOf("PDF reader") >= 0)
                    ? e.message
                    : "The PDF could not be opened. It may be damaged, protected or not a PDF file.";
            if (!cancelledPdf[id])
                Module.classboardPdfDone(id, message);
        }
        delete cancelledPdf[id];
    }

    // ---------------------------------------------------------------------- dropping files

    function canvasPosition(event) {
        var canvas = document.getElementById("qtcanvas");
        var rect = canvas ? canvas.getBoundingClientRect() : { left: 0, top: 0 };
        return { x: Math.round(event.clientX - rect.left), y: Math.round(event.clientY - rect.top) };
    }

    function hasFiles(event) {
        var types = event.dataTransfer && event.dataTransfer.types;
        return !!types && Array.prototype.indexOf.call(types, "Files") >= 0;
    }

    document.addEventListener("dragover", function (event) {
        if (!hasFiles(event))
            return;
        event.preventDefault();
        event.dataTransfer.dropEffect = "copy";
        document.body.classList.add("dropping");
    });
    document.addEventListener("dragleave", function (event) {
        if (!event.relatedTarget)
            document.body.classList.remove("dropping");
    });
    document.addEventListener("drop", async function (event) {
        if (!hasFiles(event))
            return;
        event.preventDefault();
        document.body.classList.remove("dropping");
        if (!appReady())
            return;
        var pos = canvasPosition(event);
        var files = Array.prototype.slice.call(event.dataTransfer.files || []);
        for (var i = 0; i < files.length; ++i) {
            try {
                var buffer = await files[i].arrayBuffer();
                Module.classboardDropFile(files[i].name, new Uint8Array(buffer));
            } catch (e) {
                console.warn("Could not read dropped file", files[i].name, e);
            }
        }
        Module.classboardDropDone(pos.x, pos.y);
    });

    // ------------------------------------------------------------------------------ public API

    window.classboardWeb = {
        mountStorage: mountStorage,
        persist: persist,
        isFullScreen: isFullScreen,
        setFullScreen: setFullScreen,
        canRenderPdf: function () { return typeof window.fetch === "function" && typeof Promise !== "undefined"; },
        renderPdf: function (id, bytes, dpi) { renderPdf(id, bytes, dpi); },
        cancelPdf: function (id) { cancelledPdf[id] = true; }
    };
})();
