# Driverslog (Fahrtenbuch)

A dual-platform mileage logging app for **Android** and **Windows**, built with Qt/C++.

## Features

- Trip recording (date, start/destination address, distance, driver, notes)
- Distance calculation via OSRM (free, no account) or OpenRouteService (optional API key)
- Address book with geocoding (Nominatim/OpenStreetMap)
- Driver management (optional multi-driver mode)
- PDF and CSV export
- Wi-Fi synchronisation between Android and Windows (same local network, no cloud)
- Multilingual: German, English, French, Dutch, Spanish

## Platforms

| Platform | Qt UI | Min. Version |
|---|---|---|
| Android | QML / QQuickWidget | Android 7.0 (API 24) |
| Windows | Qt Widgets | Windows 10 |

## Build Requirements

| Component | Version |
|---|---|
| Qt | 6.8.3 |
| Android NDK | 26.1.10909125 |
| Android Build Tools | 35.0.1 |
| Java (Android) | JDK 17 (Eclipse Adoptium) |
| Gradle | 8.7 |
| AGP | 8.3.2 |

## Build Instructions

### Windows

1. Open `Fahrtenbuch.pro` in Qt Creator
2. Select kit **Desktop Qt 6.8.3 MinGW 64-bit**
3. Build → Release

### Android

1. Open `Fahrtenbuch.pro` in Qt Creator
2. Select kit **Qt 6.8.3 for Android arm64-v8a**
3. Configure keystore under Projects → Android → Build Steps → Sign package
4. Build → Release
5. Run `python rename_aab.py` to copy the signed `.aab` to `build/`

### Distance Calculation

- **Default (no key required):** OSRM – `router.project-osrm.org`
- **Optional:** OpenRouteService (ORS) – enter your free API key under Settings → Distance Calculation  
  Register at [openrouteservice.org](https://openrouteservice.org/dev/#/signup) (500 requests/day free)

## Privacy Policy

Available at: [kloesel.github.io/Fahrtenbuch](https://kloesel.github.io/Fahrtenbuch/)

All trip data is stored locally on the device. No data is transmitted to the developer.  
External services used (on user initiative only): Nominatim (geocoding), OSRM or ORS (routing).

## License

This application is licensed under the **GNU General Public License v3 (GPL-3.0)**.

Qt libraries are used under the **GNU Lesser General Public License v3 (LGPL-3.0)**.  
See [`LICENSE_Qt_LGPL.txt`](LICENSE_Qt_LGPL.txt) for the full Qt LGPL license text.

Qt is dynamically linked in all builds. The Qt source code is available at  
[qt.io/download-open-source](https://www.qt.io/download-open-source/).

## Third-Party Services

| Service | Purpose | Account required |
|---|---|---|
| [Nominatim](https://nominatim.org/) | Address → GPS coordinates | No |
| [OSRM](https://project-osrm.org/) | Route distance calculation | No |
| [OpenRouteService](https://openrouteservice.org/) | Route distance (optional) | Yes (free) |
