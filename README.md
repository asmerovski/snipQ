# snipQ

A cross-platform code snippet manager inspired by [massCode](https://masscode.io/), built with **Qt 6**, **C++17**, and **SQLite**.

Runs natively on **Windows**, **Linux**, and **macOS** — no Electron, no web runtime.

---

## ✨ Features

| Feature | Details |
|---|---|
| **Sidebar tree** | Library (Favourites, All Snippets, Bin) · Folders · Tags |
| **Multi-tab snippets** | Each snippet can have multiple code fragments (tabs) |
| **Language tagging** | 37 languages in the combo (bash, Python, Rust, TypeScript, …) |
| **Tag system** | Inline tag chips on every snippet, filterable from sidebar |
| **Auto-save** | Debounced 800 ms auto-save while you type |
| **Export / Import** | Full JSON round-trip (snippets + folders + tabs + tags) |
| **Movable storage** | Relocate the SQLite DB to any path — NAS, external drive, etc. |
| **Cloud sync** | Google Drive · OneDrive · Dropbox · Box · FTP/FTPS · SMB/Samba |
| **Dark theme** | GitHub Dark–inspired palette, monospace editor |
| **Bin / restore** | Soft-delete with restore, permanent delete from Bin view |

---

## 📐 Architecture

```
snipQ/
├── CMakeLists.txt
├── resources/
│   └── resources.qrc
└── src/
    ├── main.cpp
    ├── mainwindow.{h,cpp}       # Top-level shell, menu bar, wires everything
    ├── theme.h                  # Full QSS dark theme + color constants
    ├── database.{h,cpp}         # SQLite via QtSql — all CRUD + import/export
    ├── importexport.{h,cpp}     # JSON file I/O wrapper
    ├── sidebar.{h,cpp}          # Tree sidebar (Library / Folders / Tags)
    ├── snippetlist.{h,cpp}      # Middle panel — list + search
    ├── snippeteditor.{h,cpp}    # Right panel — title, tabbed editor, tags bar
    ├── models/
    │   ├── snippet.h            # Snippet + SnippetTab structs
    │   └── folder.h             # Folder struct
    ├── dialogs/
    │   ├── newfolderdialog.{h,cpp}
    │   ├── newsnippetdialog.{h,cpp}
    │   ├── settingsdialog.{h,cpp}    # Movable storage UI
    │   └── cloudsyncdialog.{h,cpp}   # Cloud provider config + sync trigger
    └── sync/
        ├── syncmanager.{h,cpp}  # Dispatches to per-provider sync
        ├── ftpsync.{h,cpp}      # FTP/FTPS via Qt Network (PUT)
        └── smbsync.{h,cpp}      # SMB via UNC (Win) or smbclient (Linux/macOS)
```

---

## 🔧 Prerequisites

| Tool | Version |
|---|---|
| CMake | ≥ 3.21 |
| Qt | 6.4+ (Core, Gui, Widgets, Sql, Network) |
| C++ compiler | MSVC 2022 / GCC 12+ / Clang 14+ |

### Installing Qt

**Linux (Arch / CachyOS)**
```bash
sudo pacman -S qt6-base qt6-tools
```

**Linux (Ubuntu 24.04)**
```bash
sudo apt install qt6-base-dev qt6-tools-dev cmake build-essential
```

**Windows**
Download Qt Online Installer from https://www.qt.io/download-qt-installer  
Select: Qt 6.x → Desktop (MSVC 2022 64-bit)

**macOS**
```bash
brew install qt cmake
```

---

## 🏗️ Building

### Linux / macOS

```bash
git clone https://github.com/you/snipQ
cd snipQ
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/snipQ
```

### Windows (MSVC)

```powershell
# In a Developer Command Prompt for VS 2022
cmake -B build -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2022_64"
cmake --build build --config Release
.\build\Release\snipQ.exe
```

### Windows (MinGW via Qt Creator)

Open `CMakeLists.txt` in Qt Creator → Configure → Build.

### Packaging

**Linux AppImage** (using linuxdeploy):
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
DESTDIR=AppDir cmake --install build
linuxdeploy --appdir AppDir --plugin qt --output appimage
```

**macOS .dmg** (using macdeployqt):
```bash
macdeployqt build/snipQ.app -dmg
```

**Windows installer** (using windeployqt + NSIS/Inno Setup):
```powershell
windeployqt .\build\Release\snipQ.exe
# Then wrap with Inno Setup or WiX
```

---

## 📦 Database

snipQ stores all data in a single **SQLite** file (default locations):

| Platform | Default path |
|---|---|
| Linux | `~/.local/share/snipQ/snipQ.db` |
| macOS | `~/Library/Application Support/snipQ/snipQ.db` |
| Windows | `%APPDATA%\snipQ\snipQ\snipQ.db` |

### Schema

```sql
CREATE TABLE folders (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    name      TEXT NOT NULL,
    parent_id INTEGER DEFAULT -1
);

CREATE TABLE snippets (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL DEFAULT 'Untitled',
    description TEXT DEFAULT '',
    tags        TEXT DEFAULT '',          -- comma-separated
    folder_id   INTEGER DEFAULT -1,
    is_favorite INTEGER DEFAULT 0,
    is_deleted  INTEGER DEFAULT 0,
    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL
);

CREATE TABLE snippet_tabs (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    snippet_id INTEGER NOT NULL REFERENCES snippets(id) ON DELETE CASCADE,
    tab_index  INTEGER NOT NULL DEFAULT 0,
    name       TEXT NOT NULL DEFAULT 'Fragment 1',
    content    TEXT DEFAULT '',
    language   TEXT DEFAULT 'plaintext'
);
```

---

## 🔄 Export / Import (JSON)

**File → Export JSON…** creates a portable JSON file:

```json
{
  "version": 1,
  "exportedAt": "2025-06-03T12:00:00Z",
  "folders": [
    { "id": 1, "name": "Web", "parentId": -1 }
  ],
  "snippets": [
    {
      "id": 1,
      "name": "Fetch with retry",
      "description": "Exponential back-off fetch",
      "tags": ["js", "network"],
      "folderId": 1,
      "isFavorite": true,
      "isDeleted": false,
      "createdAt": "2025-01-01T10:00:00Z",
      "updatedAt": "2025-06-01T15:30:00Z",
      "tabs": [
        { "name": "fetch.ts", "content": "async function ...", "language": "typescript" }
      ]
    }
  ]
}
```

**File → Import JSON…** merges snippets into the existing database (non-destructive).

---

## ☁️ Cloud Sync

Go to **Sync → Cloud Sync…**

### OAuth providers (Google Drive, OneDrive, Dropbox, Box)

1. Select the provider.
2. Click **Authenticate →** — your browser opens the provider's login page.
3. After authorising, paste the access token into the field.
4. Click **Sync Now**.

The DB file (`snipQ_db.json` export) is uploaded to the root of your cloud storage.

> **Note:** For production use, register your own OAuth client ID at the respective developer consoles and embed it before the `Authenticate` button opens the URL. The app currently uses the generic OAuth endpoint — you must supply your own token for the sync to work.

### FTP / FTPS

Fill in host, port (default 21), credentials, and the remote file path.  
Qt's `QNetworkAccessManager` handles the FTP PUT request. For FTPS, ensure your Qt build includes SSL support (OpenSSL on Windows/Linux, Secure Transport on macOS).

### SMB / Samba

| Platform | Method |
|---|---|
| Windows | Direct UNC path copy (`\\host\share\path`) |
| Linux / macOS | Calls `smbclient` — install via `samba-client` / `brew install samba` |

---

## 🗄️ Movable Storage

**File → Settings → Storage Location**

1. Click **Browse…** to pick a new folder.
2. Click **Move Here** — the DB is copied to the new path.
3. Restart snipQ — it will use the new location on startup.

Useful for:
- Keeping snippets on a NAS or external drive
- Sharing the DB between a desktop and laptop via a shared folder
- Syncing via any third-party sync client (Syncthing, Resilio, etc.)

---

## 🎨 Theming

All styles live in `src/theme.h` as a single QSS string. Modify the `Theme::StyleSheet` constant and recompile to customise colours, fonts, spacing, and more.

---

## 📄 License

MIT — see [LICENSE](LICENSE)
