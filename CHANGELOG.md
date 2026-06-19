# Driver's Log – Changelog

## [1.6.02] – 19.06.2026

### Fehlerbehebungen
- Export/Protokoll: Start- und Zieladresse bei GPS-Fahrten war immer die Standardadresse statt dem eingetragenen Text
- Synchronisation: mehrere Ursachen für falsche oder fehlende Fahrtzweck-Zuordnung nach Sync behoben
- Synchronisation: Geräte erkannten sich nach Löschen aller Fahrten fälschlich als "neu" und verloren dabei eigene Änderungen
- GPS-Fahrt erfassen: Fahrer-Auswahl wurde bei "Mehrere Fahrer" nicht gespeichert
- GPS-Fahrt erfassen: verzerrte Darstellung nach Tastatureingabe behoben
- Entwickleroptionen blieben nach Neuinstallation eines Geräts deaktiviert
- Info-Dialog zeigte teilweise englische Texte trotz eingestellter deutscher Sprache

---

## [1.6.01] – 14.06.2026

### Neu
- GPS-Fahrt nachträglich erfassen: Toggle im manuellen Fahrtformular
- Freitext-Felder für Start und Ziel bei GPS-Aktivitäten

### Geändert
- Abbrechen-Button im GPS-Tracking-Dialog größer

### Fehlerbehebungen
- Fahrtzweck „Fahrradtour" / „Wandern" nach Neuinstall + Sync doppelt eingetragen
- Falsche Fahrtzwecke nach Sync (ID-Konflikte bei Neuinstallation)
- Aktivitätssymbol bei manueller GPS-Eingabe immer 🚗 statt 🚴/🥾
- Start/Ziel-Felder wurden nicht gespeichert
- Eingabefelder beim Bearbeiten einer GPS-Fahrt leer
- Tastatur schob Felder nicht in den sichtbaren Bereich

---

## [1.6.00] – 11.06.2026

### Neu
- **GPS-Tracking** — Strecke automatisch per GPS erfassen
  - Aktivitätsauswahl: Autofahrt 🚗, Fahrradtour 🚴, Wandern 🥾
  - Hintergrund-Tracking (auch bei minimierter App)
  - Nach Stop: Fahrtformular vorausgefüllt (Datum, km, Fahrtzweck, Adressen)
  - Aktivitätssymbol in der Fahrtenliste
- Hilfe und Info aktualisiert

### Geändert
- Hin & Zurück und Berechnen-Button bei GPS-Fahrten deaktiviert

### Fehlerbehebungen
- Einstellungen: Standard-Fahrer und Standard-Adresse wurden nicht gespeichert
- GPS-Fahrten wurden nach dem Speichern nicht in der Liste angezeigt
- Fahrtzweck „Fahrradtour" / „Wandern" wurde nicht automatisch angelegt

---

## [1.5.07] – 08.06.2026

### Fehlerbehebungen
- In-App-Kauf wird jetzt korrekt bestätigt – kein automatischer Abbruch mehr
- Synchronisation startet nach App-Neustart zuverlässiger
- Stabilitätsverbesserungen beim Billing

---

## [1.5.06] – 05.06.2026

### Fehlerbehebungen
- Weißer Bildschirm behoben: App startet nach längerer Hintergrundzeit wieder korrekt
- Kurzes Aufflackern von übergroßem Text beim Tab-Wechsel behoben
- Kauf-Status wird nach Erstattung ohne App-Neustart aktualisiert
- Interne Bibliothek aktualisiert (Google Play Hinweis behoben)

---

## [1.5.05] – 01.06.2026

### Fehlerbehebungen
- Datenbank importieren: ungültige Dateien werden erkannt und abgewiesen
- Datenbank importieren: importierte Daten werden korrekt mit anderen Geräten synchronisiert
- Datenbank exportieren: Backup enthält jetzt zuverlässig alle Daten
- In-App-Kauf: kein erneuter Kaufdialog mehr nach Deinstallation und Neuinstallation
- Formulare (Fahrt, Adresse, Fahrer, Fahrtzweck): Eingabefeld scrollt beim Antippen
  zuverlässig über die Tastatur

---

## [1.5.04] – 19.05.2026

### Neu
- Datenbank exportieren und importieren (Einstellungen → Datenbank, Android + Windows)
- Sync zurücksetzen im Sync-Log (Android + Windows)
- Windows: Datenbank-Sicherung beim Deinstallieren

### Fehlerbehebungen
- Fahrt anlegen: Felder (km, Adresse, Fahrer, Fahrtzweck, Bemerkung) werden beim Wechsel des Datums oder beim Hinzufügen neuer Einträge nicht mehr zurückgesetzt
- Entfernungsberechnung zuverlässiger bei langen Straßennamen
- Entfernungsberechnung: fehlgeschlagenes Geocoding zeigt Fehlermeldung statt falscher Kilometeranzahl
- In-App-Kauf: Bestätigung wird jetzt korrekt verarbeitet (keine automatische Stornierung mehr)
- Android 15/16: Layout-Problem auf Samsung S22 behoben
- Export: Fahrtzweck-Liste scrollbar; „Alle"-Checkbox ergänzt
- Zurück-Taste: App geht in den Hintergrund statt zu schließen

---

## [1.5.03] – 11.05.2026

Erstveröffentlichung im Google Play Store.

### Features
- Fahrtenerfassung: Datum, Start/Ziel, Entfernung, Fahrer, Fahrtzweck, Bemerkung
- Adressbuch mit automatischer Geocodierung (OSRM, kostenlos, kein Account nötig)
- Fahrerverwaltung (Mehrfahrer-Modus) – Pro
- Fahrtzweck-Kategorien – Pro
- CSV- und PDF-Export mit Filter – Pro
- WLAN-Sync zwischen Android und Windows (ohne Cloud) – Pro
- Mehrsprachig: Deutsch, Englisch, Französisch, Niederländisch, Spanisch
- Keine Werbung, keine Datenweitergabe, keine Cloud

---

## Windows-Version

Die kostenlose Windows-Desktop-App steht für Pro-Nutzer als Installer zum Download bereit:  
[github.com/Kloesel/DriversLog-Public/releases](https://github.com/Kloesel/DriversLog-Public/releases)
