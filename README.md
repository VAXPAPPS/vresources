# VResources System Monitor

VResources is a native, high-performance, dark glassmorphic system resources and processes monitor built in **C** using **GTK 4** and **Cairo**. It is designed with a zero-dependency architecture to ensure a minimal binary footprint and extreme runtime efficiency.

![VResources Preview](https://raw.githubusercontent.com/VAXPAPPS/vresources/main/logo.svg)

---

## Key Features

- **Frosted-Glass Aesthetics**: Modern translucent user interface utilizing GTK4 CSS styling (`rgba(0, 0, 0, 0.3)`) and smooth layout animations.
- **Cairo-Driven Visualizations**: High-framerate real-time charts featuring rolling timelines with neon gradients, glow trails, and radial progress gauges.
- **Animated Liquid Battery**: Custom Cairo drawing math rendering a smooth, floating liquid wave representing the current battery charge state.
- **Advanced Process Telemetry**:
  - Interactive sorting by name, PID, CPU, RAM, GPU, and Cache.
  - "All Users" process ownership toggle switch.
  - Real-time in-place row matching algorithm to prevent scroll position and row selection resets.
  - System desktop icon theme integration next to process names.
  - Dynamic telemetry refresh-rate intervals (Live/100ms, 0.5s, 1s/default).
- **Process Diagnostic Context Menu**:
  - Comprehensive **Properties panel** detailing 18 distinct process metrics, memory mapping (`/proc/PID/maps`), and open file descriptors (`/proc/PID/fd`).
  - Diagnostic tools to change Niceness level, set CPU Core Affinity, and dispatch Unix control signals (Stop, Continue, Terminate, Kill).
- **Venom Theme Manager Integration**: Direct parsing and automatic file monitoring of theme settings (`~/.config/venom/settings.vaxp`) to hot-reload custom colors.
- **Header Bar (CSD) Integration**: Modern GTK4 Client-Side Decorations showing search/filtering controls inside the header bar dynamically, maximizing client area when active.
- **Demo Mode Simulator**: Integrated simulation toggle switch to preview visual charts, animations, and telemetry in virtual machines or headless setups.

---

## Project Structure

- `src/`: Source code modules (.c files).
- `include/`: Header files (.h files).
- `Makefile`: Build settings and compiler configurations.
- `style.css`: Transparent glassmorphic CSS rules.
- `logo.svg`: Scalable vector icon.
- `package_deb.sh`: Packaging script for Debian systems.

---

## Build and Compilation

### Prerequisites
Make sure you have GTK4 development libraries, GCC, and Make installed:
```bash
sudo apt update
sudo apt install build-essential libgtk-4-dev
```

### Build Instructions
To compile the project:
```bash
make
```

To run the application locally:
```bash
./vresources
```

To clean compile artifacts:
```bash
make clean
```

---

## Debian Packaging & Installation

You can package and register the application system-wide as a desktop application.

1. **Build the Debian package**:
   ```bash
   ./package_deb.sh
   ```
   This generates `vresources_0.1.0_amd64.deb` directly in the project root.

2. **Install the package**:
   ```bash
   sudo dpkg -i vresources_0.1.0_amd64.deb
   ```

Installing the package automatically registers:
- Binary path: `/usr/bin/vresources`
- Stylesheet assets: `/usr/share/vresources/style.css`
- Desktop Launcher: `/usr/share/applications/vresources.desktop`
- Scalable Icon: `/usr/share/icons/hicolor/scalable/apps/vresources.svg`

---

## VAXP Ecosystem

**VResources** is a core utility and an integral part of the **VAXP OS Application Ecosystem**, developed to adhere to the organization's standards for lightweight, efficient, responsive, and beautifully integrated applications.
