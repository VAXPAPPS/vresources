# VResources Developer Documentation

This document provides a technical walkthrough of the **VResources** codebase, describing the roles of key files and instructing developers on how to modify or extend specific features of the application.

---

## 1. Directory Structure

```
VResources/
├── include/                 # Header files
│   ├── charts.h
│   ├── process_actions.h
│   ├── process_reader.h
│   ├── process_view.h
│   ├── system_reader.h
│   ├── theme_manager.h
│   └── ui.h
├── src/                     # C Implementation files
│   ├── charts.c
│   ├── main.c
│   ├── process_actions.c
│   ├── process_reader.c
│   ├── process_view.c
│   ├── system_reader.c
│   ├── theme_manager.c
│   └── ui.c
├── Makefile                 # GNU Make compilation configuration
├── style.css                # Glassmorphic stylesheet
├── logo.svg                 # Application vector icon asset
└── package_deb.sh           # Debian packaging script
```

---

## 2. Core Modules Reference

### 2.1 entrypoint: `src/main.c`
- **Role**: Launches the GTK application, registers the CSS providers, sets up refresh timers, and coordinates the active telemetry interval.
- **Timers**:
  - `on_telemetry_tick()`: Triggered periodically to refresh system and process statistics.
  - `on_animation_tick()`: Triggered every 50ms (20 FPS) to advance the battery's liquid wave phase.

### 2.2 UI Layout: `src/ui.c`
- **Role**: Assembles the GTK 4 layout using client-side decorations (`GtkHeaderBar`), navigation toggles, sidebar widgets, and stacks.
- **Dynamic CSD Header**: Implements the tab toggled event handler (`on_tab_toggled`) which shows or hides the process search and filtering widgets inside the header bar, and updates the title based on the active tab page.
- **Battery Animation**: Hooked to a custom Cairo drawing callback (`draw_liquid_battery_cb`) to render the wave effect.

### 2.3 Visual Charting: `src/charts.c`
- **Role**: Custom Cairo (`cairo_t`) rendering engine. Handles drawing the rolling area charts (e.g. CPU load, Network bandwidth) and circular radial gauges.
- **Properties**: Manages scale limits, gridlines, color gradients, and line neon-glow trails.

### 2.4 System Telemetry: `src/system_reader.c`
- **Role**: Directly parses Linux kernel interfaces to collect host-level telemetry data:
  - CPU usage: `/proc/stat`
  - Memory usage: `/proc/meminfo`
  - Intel/AMD GPU usage: `/sys/class/drm/card*/device/gpu_busy_percent`
  - NVIDIA GPU usage: Dynamically parses output of `nvidia-smi`
  - Network throughput: `/proc/net/dev`
  - Storage I/O: `/proc/diskstats` and `statvfs`
  - Battery stats: `/sys/class/power_supply/`
- **Demo Mode**: Includes high-fidelity simulated telemetry generators (`generate_simulated_stats()`) used when hardware access is restricted or demo mode is toggled ON.

### 2.5 Process Telemetry: `src/process_reader.c`
- **Role**: Reads `/proc` directories to build process logs.
- **Process Ownership**: Performs a lightweight `stat()` system call on the `/proc/PID` directory to extract the process owner's UID (`st.st_uid`).

### 2.6 Process Table UI: `src/process_view.c`
- **Role**: Configures the process grid using `GtkTreeView`. Implements in-place list store updates (`update_process_view()`) to prevent scroll jumps on telemetry ticks. Handles search querying, "All Users" UID matching filters, and refresh rate callbacks.

### 2.7 Context Menu Actions: `src/process_actions.c`
- **Role**: Spawns UI dialogs (Niceness sliders, Core Affinity checkbox grids, UNIX signals emitter, raw Process Maps viewer, and File Descriptors lister).
- **Process Properties**: Implements the comprehensive 18-metric properties panel.

### 2.8 Theme settings: `src/theme_manager.c`
- **Role**: Integrates the VAXP venom theme settings engine. Reads background and foreground configuration colors from `~/.config/venom/settings.vaxp` and uses `GFileMonitor` to monitor and trigger on-the-fly updates.

---

## 3. How to Modify Features

### 3.1 Adjusting CSS Styles & Glassmorphism
- **Where to edit**: `style.css`
- **Mechanism**: The stylesheet is loaded in `src/ui.c` via `ui_load_css()`. During development, it falls back to checking the local working directory. Upon installation via Debian packages, it checks the path `/usr/share/vresources/style.css`.
- **How to edit**: Update the `.card`, `.nav-btn`, or `window.main-window` classes inside `style.css` to modify translucent backgrounds, margins, or active glow colors.

### 3.2 Modifying the Telemetry Tick Rates
- **Where to edit**: `src/main.c` & `src/process_view.c`
- **Mechanism**: The default update interval is 1 second (1000ms). When a user changes the refresh dropdown in the UI:
  - `on_refresh_interval_changed()` in `src/process_view.c` maps the selected index to an interval (0/Live = 100ms, 0.5s = 500ms, 1s = 1000ms).
  - It triggers `ui_set_telemetry_interval()` in `src/main.c`, which removes the existing GLib timer source (`g_source_remove`) and registers a new one (`g_timeout_add`).
- **How to edit**: Modify the mapped intervals inside `on_refresh_interval_changed()` in `src/process_view.c` to add alternative tick rates.

### 3.3 Adding Columns to the Process View Grid
- **Where to edit**: `include/process_view.h` & `src/process_view.c`
- **Mechanism**: Columns are defined using an enum mapping to `GtkListStore` fields.
- **How to edit**:
  1. Add a new index (e.g., `COL_MY_METRIC`) to the column enum inside `include/process_view.h`.
  2. Increase the column count in `gtk_list_store_new` inside `create_process_view()` (`src/process_view.c`).
  3. Instantiate a new `GtkCellRendererText` and append the column to the treeview using `gtk_tree_view_append_column`.
  4. Update the insertion loop in `update_process_view()` to fetch the metric from the `ProcessInfo` object and insert it into the list store row.

### 3.4 Customizing Process Icon Mappings
- **Where to edit**: `src/process_view.c`
- **Mechanism**: Icons are resolved by `get_process_icon_name()`. It checks a hardcoded list of common process names first, then queries the local `GtkIconTheme` for matching desktop names.
- **How to edit**: Locate the `struct IconMapping` array inside `get_process_icon_name()` and add standard key-value mappings (e.g. `{"myprocess", "system-run-symbolic"}`).

### 3.5 Editing Context Menu Actions
- **Where to edit**: `src/process_actions.c`
- **Mechanism**: The context menu is built inside `show_process_context_menu()` using a GTK popover.
- **How to edit**:
  - Add a button or menu action widget inside `show_process_context_menu()`.
  - Wire its click event to a callback function.
  - Make sure to copy the telemetry context variables locally before popping down the menu to prevent memory access issues.

### 3.6 Extending Telemetry Data Gathering
- **Where to edit**: `src/system_reader.c` & `include/system_reader.h`
- **Mechanism**: `SystemStats` struct inside `include/system_reader.h` holds system state.
- **How to edit**:
  - Add a field inside `SystemStats`.
  - Implement parsing logic inside `system_reader_update()` in `src/system_reader.c` by opening relevant `/proc` or `/sys` files.
  - Implement mock generation data inside `generate_simulated_stats()` to maintain a consistent output when in Demo Mode.

### 3.7 Modifying Debian Package Metadata
- **Where to edit**: `package_deb.sh`
- **Mechanism**: The script sets up paths, file permissions, desktop launchers, and control settings before executing `dpkg-deb --build`.
- **How to edit**: Modify variables like `PKG_VERSION`, `PKG_NAME`, or the `Maintainer` and `Depends` fields inside the generated `DEBIAN/control` block.
