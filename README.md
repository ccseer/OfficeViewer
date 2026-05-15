# OfficeViewer

## Building and Running

Requirements: Qt 6.8, CMake 3.16+.

1. **Clone the Repository**

   ```bash
   git clone https://github.com/ccseer/OfficeViewer.git
   cd OfficeViewer
   ```

2. **Download Oracle Outside In Technology SDK**

   Download the SDK from [SDK Release](https://github.com/ccseer/OfficeViewer/releases/tag/SDK) and extract to the `sdk` directory.

3. **Build**

   ```bash
   cmake -B build
   cmake --build build
   ```

   This produces two outputs:
   - `officeviewer.dll` — the Seer plugin
   - `test_officeviewer.exe` — standalone viewer for testing

## Seer Plugin

OfficeViewer is a file preview plugin for [Seer](https://1218.io) — a quick-look tool for Windows.

- Supports previewing Microsoft Office files (`.doc`, `.docx`, `.xls`, `.xlsx`, `.ppt`, `.pptx`, etc.)
- Supports additional formats: `.pdf`, `.dwg`, `.vsd`, `.rtf`, and more
- Built on Oracle Outside In Technology for high-fidelity rendering
- Built as a native DLL plugin for Seer 4.0.0+

Visit [1218.io](https://1218.io) to download Seer and learn more about the plugin ecosystem.

## todo:

- add support for ctrl + wheel, ctrl+ -/=
