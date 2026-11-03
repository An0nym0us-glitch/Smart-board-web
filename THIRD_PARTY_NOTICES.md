# Third-party notices

ClassBoard itself is licensed under the MIT License (see `LICENSE`). The Windows installer and
the portable download also contain the following third-party software, each under its own
licence. The full licence texts are in the `licenses` folder next to this file.

## Qt 5.15

* Copyright © The Qt Company Ltd. and other contributors.
* Used under the **GNU Lesser General Public License version 3** (`licenses/LGPL-3.0.txt`,
  which builds on `licenses/GPL-3.0.txt`).
* ClassBoard uses the Qt Core, Gui, Widgets and Svg modules and the plugins `windeployqt`
  deploys with them. They are dynamically linked: the Qt DLLs and plugins in the `bin`
  folder are unmodified and can be replaced with other compatible builds of Qt 5.15.
* Source code: <https://download.qt.io/archive/qt/5.15/> (Qt 5.15.2) and
  <https://code.qt.io/>.
* Qt includes third-party components (for example zlib, libpng, PCRE2, HarfBuzz, FreeType and
  double-conversion) under their own permissive licences; see
  <https://doc.qt.io/qt-5/licenses-used-in-qt.html>.

## Poppler (PDF import)

* Copyright © the Poppler authors.
* Licensed under the **GNU General Public License** (version 2 or later / version 3;
  `licenses/GPL-2.0.txt`, `licenses/GPL-3.0.txt`).
* Shipped as separate, unmodified programs and libraries in `bin\poppler\bin`
  (`pdftoppm.exe` and the DLLs it needs). ClassBoard runs `pdftoppm` as an external program to
  turn PDF pages into images; it is not linked into ClassBoard.
* The Windows build comes from <https://github.com/oschwartz10612/poppler-windows> (release
  24.08.0-0), which also documents how it is built. Poppler source code:
  <https://poppler.freedesktop.org/>.
* The libraries bundled with Poppler (such as Cairo, FreeType, Fontconfig, libpng, libjpeg,
  libtiff, OpenJPEG, Little CMS and zlib) are under their own open-source licences; the licence
  files that come with that build are copied to `bin\poppler\licenses`.

## Microsoft Visual C++ runtime

* `msvcp140*.dll`, `vcruntime140*.dll` and `concrt140.dll` in the `bin` folder are the
  Microsoft Visual C++ redistributable runtime, distributed under the Microsoft Visual Studio
  licence terms for redistributable code.

## Programs ClassBoard can use but does not include

* **Microsoft PowerPoint** or **LibreOffice**, if installed, convert PowerPoint presentations
  for import. They are not part of ClassBoard and are used under their own licences.
