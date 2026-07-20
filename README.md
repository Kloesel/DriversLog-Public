# Driver's Log (Fahrtenbuch)

A dual-platform activity logging app for **Android** and **Windows**, built with Qt/C++.  
Record car trips, cycling tours and hikes — with optional GPS tracking on Android.

## Download

- **Android:** [Google Play Store](https://play.google.com/store/apps/details?id=de.kloesel.driverslog)
- **Windows:** [Latest release](https://github.com/Kloesel/DriversLog-Public/releases/latest)

## Free vs. Pro (Android)

The Android app is free to install. A one-time **In-App Purchase** ("Pro") unlocks all features:

| Feature | Free | Pro |
|---|---|---|
| Trip / activity recording | max. 25 entries | unlimited |
| GPS tracking (car, cycling, hiking) | max. 25 entries | unlimited |
| Export (CSV & PDF) | ✗ | ✓ |
| Driver management | ✗ | ✓ |
| Trip purpose management | ✗ | ✓ |
| Wi-Fi sync | ✗ | ✓ |
| Windows desktop app | ✗ | ✓ free |

**Windows version is always free** – available as a standalone installer at  
[github.com/Kloesel/DriversLog-Public/releases](https://github.com/Kloesel/DriversLog-Public/releases)

## Features

- **GPS tracking** (Android) — automatic distance recording for car trips 🚗, cycling tours 🚴 and hikes 🥾
  - Background tracking via Foreground Service (works with the app minimised)
  - Activity selection with optimised GPS intervals and drift filters
  - Automatic address lookup from your address book
  - Trip form pre-filled with date, distance and trip purpose after stopping
- Manual trip recording (date, start/destination, distance, driver, trip purpose, notes)
- Distance calculation via OSRM (free, no account) or OpenRouteService (optional API key)
- Address book with geocoding (Nominatim/OpenStreetMap)
- Driver management (optional multi-driver mode) — *Pro*
- Trip purpose management — *Pro*
- PDF and CSV export — *Pro*
- Wi-Fi synchronisation between Android and Windows (same local network, no cloud) — *Pro*
- Automatic database backup with safeguards against accidental data loss, plus manual restore from any previous backup
- Modern edge-to-edge design on Android, with official Android 16 support
- Multilingual: German, English, French, Dutch, Spanish
- No ads, no data collection, no cloud

## Platforms

| Platform | Qt UI | Min. Version |
|---|---|---|
| Android | QML / QQuickWidget | Android 9.0 (API 28), officially supports Android 16 (API 36) |
| Windows | Qt Widgets | Windows 10 |

## In-App Purchase

The Android app uses **Google Play Billing** for the one-time Pro purchase.  
Product ID: `driverslog_pro`

## Privacy Policy

Available at: [kloesel.github.io/DriversLog-Public](https://kloesel.github.io/DriversLog-Public/)

All trip data is stored locally on the device. No data is transmitted to the developer.  
GPS location data is processed entirely on-device while a trip is being recorded and is
never transmitted during that process. The resulting trip data (e.g. addresses and
distance derived from GPS) may optionally be synced between your own devices via local
Wi-Fi if you set this up yourself — this data is never sent to the developer or any
third party, and never leaves your local network.  
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
