# BackUpTool

Kommandozeilen-Backup-Tool in C++17, das Verzeichnisse als volle und inkrementelle `tar.gz`-Blöcke versioniert in einen AWS-S3-Bucket sichert und zu jedem gewählten Zeitpunkt wiederherstellt.

Ein Backup-Set besteht aus einem vollen Backup und den darauf aufbauenden inkrementellen Blöcken. Erzeugt werden diese mit `tar --listed-incremental`; jeder Block wird mit Zeitstempel im Bucket abgelegt und ist damit eine eigene Version.

Beim Wiederherstellen ermittelt das Tool automatisch die passende Kette — das jüngste volle Backup vor dem gewählten Zeitpunkt plus alle inkrementellen Blöcke bis dorthin — und entpackt sie der Reihe nach. Neben der kompletten Version lassen sich auch einzelne Dateien oder Unterordner aus einer Version herausholen. Beim Löschen werden alle Blöcke entfernt, die von der gewählten Version abhängen, damit keine unvollständigen Ketten zurückbleiben.

## Voraussetzungen

- **GNU tar** — das Tool ruft `tar --listed-incremental` auf und nutzt `/dev/null` als Snapshot-Ziel beim Entpacken. Damit läuft es auf Linux und macOS (dort GNU tar als `gtar` installieren), nicht unter Windows.
- **CMake ≥ 3.21** und ein C++17-Compiler
- **AWS SDK for C++** mit der S3-Komponente
- **AWS-Credentials** mit Lese-, Schreib- und Löschrechten auf dem Ziel-Bucket
- Gesetzte `HOME`-Umgebungsvariable (bestimmt Arbeits- und Ablageordner)

## Bauen

Die Abhängigkeiten sind über ein vcpkg-Manifest (`vcpkg.json`) beschrieben:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

Ist das AWS SDK bereits systemweit installiert, reicht auch:

```bash
cmake -B build && cmake --build build
```

Das Binary liegt anschließend unter `build/BackUpTool`.

## Konfiguration

Bucket, Region und Ablagepfade stehen in [`include/Config.h`](include/Config.h) und müssen vor dem ersten Build angepasst werden:

| Feld | Bedeutung | Standard |
| --- | --- | --- |
| `bucketName` | Ziel-Bucket | fest eingetragen, anpassen |
| `region` | AWS-Region des Buckets | `eu-north-1` |
| `basePrefix` | Prefix, unter dem alle Sets liegen | `backups/` |
| `arbeitsOrdner` | lokaler Temp-Ordner für tar-Blöcke und Snapshots | `$HOME/s3backups_tmp` |

Der Arbeitsordner wird beim Start angelegt und beim Beenden des Programms wieder gelöscht.

## Credentials

Zwei Wege, das Tool authentifiziert sich mit dem ersten, der greift:

```bash
# 1) per Programmargument (Region optional)
./BackUpTool <ACCESS_KEY> <SECRET_KEY> [REGION]

# 2) ohne Argumente -> Standard-Credential-Chain des SDK (~/.aws/credentials)
./BackUpTool
```

## Benutzung

Nach dem Start erscheint ein Menü:

```
=== Backup-Tool ===
1) Vollbackup
2) Inkrementelles Backup
3) Versionen auflisten
4) Version wiederherstellen
5) Einzelne Datei wiederherstellen
6) Backup löschen
0) Beenden
```

**1 — Vollbackup.** Quellordner und Backup-Name (Set-Name) angeben. Das Tool zeigt den Ordnerinhalt als Baum und fragt vor dem Upload nach einer Bestätigung. Eine eventuell vorhandene Snapshot-Datei wird verworfen, das Set beginnt eine neue Kette.

**2 — Inkrementelles Backup.** Setzt ein vorhandenes Vollbackup unter demselben Set-Namen voraus. Das Tool lädt die `.snar`-Datei aus dem Bucket, packt nur die Änderungen seit dem letzten Block und lädt Block und aktualisierte `.snar` wieder hoch.

**3 — Versionen auflisten.** Zeigt alle Blöcke eines Sets mit Zeitstempel und Typ (`full` / `incr`).

**4 — Version wiederherstellen.** Zielordner und Version wählen. Wiederhergestellt wird nach `<Zielordner>/<Set-Name>`; existiert der Ordner schon, wird `_1`, `_2` … angehängt, bestehende Daten werden also nicht überschrieben.

**5 — Einzelne Datei wiederherstellen.** Die gewählte Version wird zunächst in einen Vorschauordner entpackt und ihr Inhalt als Baum angezeigt. Anschließend gibt man den inneren Pfad an (z.B. `TestOrdner/datei1.txt`) und den Zielordner; nur dieser Pfad wird herauskopiert.

**6 — Backup löschen.** Löscht die gewählte Version **und alle darauf aufbauenden inkrementellen Backups** bis zum nächsten Vollbackup. Ist die neueste Kette betroffen, wird zusätzlich die `.snar`-Datei entfernt — dadurch erzwingt das Tool, dass als Nächstes wieder ein Vollbackup erstellt wird.

## Ablage im Bucket

```
backups/
└── <setName>/
    ├── <setName>_2026-06-21_153000_full.tar.gz
    ├── <setName>_2026-06-22_101500_incr.tar.gz
    ├── <setName>_2026-06-23_090000_incr.tar.gz
    └── <setName>.snar
```

Der Zeitstempel hat das Format `%Y-%m-%d_%H%M%S`. Weil er lexikografisch sortierbar ist, entspricht die alphabetische Reihenfolge der Keys der zeitlichen Reihenfolge der Blöcke — darauf baut die Ketten-Ermittlung auf.

Die `.snar` ist die Snapshot-Datei von GNU tar. Sie hält fest, welchen Stand der letzte Block hatte, und ist die Grundlage für das nächste inkrementelle Backup. Pro Set existiert genau eine.

### Beispiel: Wiederherstellung einer Kette

| Version | Zeit | Typ | |
| --- | --- | --- | --- |
| V1 | 10:11 | full | |
| V2 | 11:09 | incr | |
| V3 | 12:20 | incr | ← gewählt |
| V4 | 13:00 | incr | |
| V5 | 18:00 | full | |

Gewählt wird V3. Wiederhergestellt wird die Kette **V1 → V2 → V3**, in dieser Reihenfolge in denselben Zielordner entpackt. V4 und V5 bleiben unberührt.

## Aufbau

| Klasse | Aufgabe |
| --- | --- |
| `main` | Startet die SDK-Session, liest Credentials aus den Argumenten, verdrahtet die Objekte, räumt den Temp-Ordner am Ende auf |
| `CliApp` | Menüführung, Benutzereingaben, Bestätigungsabfragen |
| `BackupService` | Fachlogik: Keys bauen, Ketten ermitteln, Backup-/Restore-/Löschabläufe orchestrieren |
| `S3Storage` | Kapselt alle AWS-Aufrufe (Upload, Download, Auflisten, Existenzprüfung, Löschen) |
| `Archiver` | Baut und führt die `tar`-Befehle aus |
| `PrintPath` | Gibt Verzeichnisbäume auf der Konsole aus |
| `Config` | Bucket, Region, Prefix, Pfade |
| `Result` | Rückgabetyp `{ erfolg, nachricht }` für alle Operationen |
| `AwsSdkSession` | RAII-Wrapper um `Aws::InitAPI` / `Aws::ShutdownAPI` |

`BackupService` kennt nur `S3Storage` und `Archiver`, nicht das AWS SDK selbst; `CliApp` kennt weder das SDK noch `tar`.
