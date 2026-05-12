# Driver's Log (Fahrtenbuch)

A dual-platform mileage logging app for **Android** and **Windows**, built with Qt/C++.

## Download

- **Android:** [Google Play Store](https://play.google.com/store/apps/details?id=de.kloesel.driverslog)
- **Windows:** [Latest release](https://github.com/Kloesel/DriversLog-Public/releases/latest)

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

## In-App Purchase

The Android app uses **Google Play Billing** for the one-time Pro purchase.  
Product ID: `driverslog_pro`

## Privacy Policy

Available at: [kloesel.github.io/DriversLog-Public](https://kloesel.github.io/DriversLog-Public/)

All trip data is stored locally on the device. No data is transmitted to the developer.  
External services used (on user initiative only): Nominatim (geocoding), OSRM or ORS (routing).

## License

This application is proprietary software. All rights reserved.

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
