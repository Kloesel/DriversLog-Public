# Driver's Log – Changelog

## [1.8.02] – 04.08.2026 (Windows + Android)

### Neu
- Neuer Button „Sync zurücksetzen" direkt unter Einstellungen → Synchronisation (Windows und Android) – bisher nur inoffiziell erreichbar

### Geändert
- Windows: Menüpunkt „Synchronisation" ist jetzt ausgeblendet (WLAN-Sync läuft ohnehin automatisch im Hintergrund, der Menüpunkt sorgte nur für Verwirrung)
- Windows: Menüpunkt „Datei → Speichern" entfernt (war überflüssig – alle Daten werden automatisch gespeichert)
- Hilfe (Windows + Android) korrigiert: Anleitung zum Sync-Zurücksetzen verweist jetzt auf den neuen Button in den Einstellungen

### Fehlerbehebungen
- Android: Nach „Datenbank wiederherstellen" erschien die App auf manchen Geräten nicht automatisch wieder im Vordergrund und musste manuell in der Übersicht angetippt werden – behoben

## [1.8.01] – 28.07.2026

### Geändert
- Preis für Fahrtenbuch Pro wird jetzt direkt im Kauf-Dialog angezeigt
- Benachrichtigungen (z. B. „Adresse gespeichert", „Fahrt gelöscht") werden jetzt zuverlässiger angezeigt

### Fehlerbehebungen
- Entfernungsberechnung schlug bei vollständigen Adressen (Straße, Hausnummer, PLZ, Ort) teils fehl – behoben
- „Datenbank sichern"-Button in den Einstellungen reagierte manchmal nicht auf Antippen – behoben
- PDF-Export: Tabellenüberschriften waren nicht mit den Werten darunter ausgerichtet
- GPS-Aufzeichnung: Distanzanzeige erscheint jetzt sofort beim Start, nicht erst nach der ersten Bewegung
- GPS-Aufzeichnung: kleinere Ungenauigkeiten durch GPS-Ungenauigkeit (Jitter) reduziert
- Einige Übersetzungslücken behoben (u. a. Info-Dialog, WLAN-Sync-Meldungen)
- Hilfe-Text verwies auf die falsche Stelle für „Adressen anlegen"

### Sonstiges
- Interne Bibliothek für In-App-Käufe aktualisiert (Google-Play-Anforderung ab 31.08.2026)

### Bekanntes Problem
Bei aktivierter Wischgesten-Navigation (statt der klassischen 3-Tasten-Navigation) reagiert das Menü-Symbol (☰) gelegentlich nach dem Zurückwischen aus einem Formular kurzzeitig nicht. Die App bleibt dabei voll funktionsfähig und stürzt nicht ab. Die Ursache liegt nachweislich im zugrunde liegenden Qt-Framework, nicht in unserem Code; wir haben das mit ausführlichen Belegen bei den Qt-Entwicklern gemeldet ([QTBUG-148335](https://bugreports.qt.io/browse/QTBUG-148335)) und warten weiterhin auf eine Rückmeldung. Bei 3-Tasten-Navigation tritt das Problem nicht auf.

---

## [1.8.0] – 17.07.2026

### Neue Funktionen
- GPS-Aufzeichnung für Fahrrad- und Wandertouren – Strecke wird direkt per GPS erfasst, statt Start/Ziel manuell einzutragen
- Edge-to-Edge-Design: Die App nutzt jetzt den vollen Bildschirm inklusive der Bereiche unter Statusleiste und Navigationsleiste
- Fahrtenliste: nach dem Bearbeiten einer Fahrt springt die Liste nicht mehr an den Anfang, sondern zeigt den geänderten Eintrag

### Fehlerbehebungen
- Kritischer Fix: Ein Fehler in der automatischen Datenbank-Sicherung konnte unter bestimmten Umständen zu Datenverlust führen – behoben
- „Letztes Backup" zeigt jetzt zusätzlich die Uhrzeit an, nicht nur das Datum
- Diverse Übersetzungslücken behoben (u. a. Qt-Standarddialoge auf Android teilweise englisch geblieben)
- Wisch-Navigation reagierte nach bestimmten Bildschirmwechseln auf Android 16 teils erst nach mehrfachem Antippen – behoben
- Adressbuch: mehrere Ziele/Personen an derselben Adresse (z.B. Hochhaus) lassen sich jetzt anlegen, ohne dass "Adresse schon vergeben" gemeldet wird
- Kompatibilität mit 16-KB-Speicherseiten (Android 16) verbessert

### Sonstiges
- Vorbereitung auf Android 16 (API 36), deutlich vor der Google-Play-Frist (31.08.2026)

### Bekanntes Problem
Bei aktivierter Wischgesten-Navigation (statt der klassischen 3-Tasten-Navigation) reagiert das Menü-Symbol (☰) gelegentlich nach dem Zurückwischen aus einem Formular kurzzeitig nicht. Die App bleibt dabei voll funktionsfähig und stürzt nicht ab – meist reicht kurzes Warten oder ein Tap woanders auf dem Bildschirm. Die Ursache liegt nachweislich nicht in unserem eigenen Code, sondern im zugrunde liegenden Qt-Framework; wir haben das bereits mit ausführlichen Belegen bei den Qt-Entwicklern gemeldet ([QTBUG-148335](https://bugreports.qt.io/browse/QTBUG-148335)). Bei 3-Tasten-Navigation tritt das Problem nicht auf.

---

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
