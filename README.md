# Driverslog (Fahrtenbuch)

A dual-platform mileage logging app for **Android** and **Windows**, built with Qt/C++.

## Free vs. Pro (Android)

The Android app is free to install. A one-time **In-App Purchase** ("Pro") unlocks all features:

| Feature | Free | Pro |
|---|---|---|
| Trip recording | max. 25 trips | unlimited |
| Export (CSV & PDF) | ✗ | ✓ |
| Driver management | ✗ | ✓ |
| Trip purpose management | ✗ | ✓ |
| Wi-Fi sync | ✗ | ✓ |
| Windows desktop app | ✗ | ✓ free |

**Windows version is always free** – available as a standalone installer at  
[github.com/Kloesel/DriversLog-Public/releases](https://github.com/Kloesel/DriversLog-Public/releases)

## Features

- Trip recording (date, start/destination, distance, driver, trip purpose, notes)
- Distance calculation via OSRM (free, no account) or OpenRouteService (optional API key)
- Address book with geocoding (Nominatim/OpenStreetMap)
- Driver management (optional multi-driver mode) — *Pro*
- Trip purpose management — *Pro*
- PDF and CSV export — *Pro*
- Wi-Fi synchronisation between Android and Windows (same local network, no cloud) — *Pro*
- Multilingual: German, English, French, Dutch, Spanish
- No ads, no data collection, no cloud

## Platforms

| Platform | Qt UI | Min. Version |
|---|---|---|
| Android | QML / QQuickWidget | Android 9.0 (API 28) |
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
- **Optional:** OpenRouteService (ORS) – enter your free API key under Settings → Distance Calculation (unlocked in release builds via 7 taps on the version number)  
  Register at [openrouteservice.org](https://openrouteservice.org/dev/#/signup) (500 requests/day free)

## In-App Purchase

The Android app uses **Google Play Billing Library 7.1.1** for the one-time Pro purchase.  
Product ID: `driverslog_pro`

Testing:
- **Debug builds**: always Pro (no restrictions)
- **Release builds**: tap version number 7× to unlock developer options → enable Pro mode for testing

## Privacy Policy

Available at: [kloesel.github.io/DriversLog-Public](https://kloesel.github.io/DriversLog-Public/)

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
| [Google Play Billing](https://developer.android.com/google/play/billing) | In-App Purchase | — |
