#include "Archiver.h"
#include <filesystem>
#include <cstdlib>
#include <iostream>

/**
 *Beispiel:
 *quellOrdner: /home/me/Bilder
 *snapshotPfad: /tmp/s3backup/bilder.snar
 *blockpfad: /tmp/s3backup/bilder_2026-06-14_153000_full.tar.gz
 *
 *----> Befehl sähe so aus: tar --listed-incremental='/tmp/s3backup/bilder.snar' -czf '/tmp/s3backup/bilder_2026-06-14_153000_full.tar.gz' -C '/home/me' 'Bilder'
 *
 *----> fertige .tar heißt dann: bilder_2026-06-14_153000_full.tar.gz
 *
 *
 */

namespace fs = std::filesystem;

Result Archiver::executeTarBefehl(const std::string &befehl) {
    std::cout << "[TAR] " << befehl << std::endl;
    int code = std::system(befehl.c_str());

    //Prüfe ob execute des Befehls erfolgreich war
    if (code==0) {
        return Result::ok();
    }
    return Result::fehler("Befehl fehlgeschlagen: " + befehl);

}


Result Archiver::tarErstellen(const std::string &quellOrdner, const std::string &snapshotPfad, const std::string &blockPfad) {
    fs::path quelle = fs::absolute(quellOrdner);

    /**
     *... -C '/tank/home/32roda1bif' 'TestOrdner2'
     *
     *... -C '/tank/home/32roda1bif/TestOrdner2' ''
     *
     *TestOrdner2/ wenn Ordner Pfad auf / endet dann nehmen Parent Pfad da sonst Tar in den Ordner rein geht und quasi sagt:
     *"packe das Ding mit dem leeren Namen ein" tar findet hier nichts mit leerem Namen -> daher leeres Archiv
     *dies verhinder wir durch den quelle.filename().empty dann sagen wir einfach nimm statt /TestOrdner/ einfach /Testordner
     */

    if (quelle.filename().empty()) quelle = quelle.parent_path(); // / -> Am Ende abfangen
    //Prüfung ob quelle ein Directory ist
    if (!fs::is_directory(quelle)) return Result::fehler("Pfad ist kein Directory");


    std::string eltern = quelle.parent_path().string();
    std::string name = quelle.filename().string();

    //Hier den Befehl zusammenbauen:
    /**
     * -c = create -> neues Archiv erstellen
     * -z = (gzip) Komprimiert das Archiv oder entpackt .tar
     * -f = (file) -> gibt den Namen der Archivdatei an (muss direkt vorm Archivnamen stehen)
     *
     * -C = Change Directory -> wechselt vor dem Ausführen in das angegebene Verzeichnis
     */
    std::string befehl = "tar --listed-incremental='" + snapshotPfad + "' -czf '" + blockPfad + "' -C '" + eltern + "' '" + name + "'";
    return executeTarBefehl(befehl);
}


Result Archiver::createFull(const std::string &quellOrdner, const std::string &snapshotPfad, const std::string &blockPfad) {

    //in ec wird ein ggf Fehlercode abgefangen
    std::error_code ec;
    fs::remove(snapshotPfad, ec);
    if (ec) return Result::fehler("Es ist ein Fehler beim löschen der alten Snapshot Datei aufgetreten: " + snapshotPfad);
    return tarErstellen(quellOrdner, snapshotPfad, blockPfad);
}

Result Archiver::createIncremental(const std::string &quellOrdner, const std::string &snapshotPfad, const std::string &blockPfad) {
    if (!fs::exists(snapshotPfad)) return Result::fehler("Es existiert noch keine Snapshot Datei. Erst ein volles Backup erstellen");
    return tarErstellen(quellOrdner, snapshotPfad, blockPfad);
}

/**
 *Befehl hier um einen Zielordner zu entpacken
 * - tar --listed-incremental=/dev/null --> hier incremental weil wenn man ein inkrementelles backup entpackt enthält das Archiv Metadaten darüber
 * welche Datenn seit dem letzen Backup gelöscht wurden -> damit tar diese Dateien auch im Zielordner löscht muss es wissen dass es ein inkrementelles Archiv liest
 *
 * - /dev/null = ist hier wie ein schwarze loch -> da tar beim Entpacken den inkrementellen Modus aktivieren soll, aber keine Snapshot Datei auf der Festplatte lesen
 * oder aktualisieren soll
 *
 * -x = entpacken
 */
Result Archiver::extractAll(const std::string &blockPfad, const std::string &zielOrdner) {
    std::error_code ec;
    fs::create_directories(zielOrdner, ec);
    if (ec) return Result::fehler("Zielordner konnte nicht erstellt werden: " + zielOrdner);
    std::string befehl = "tar --listed-incremental=/dev/null -xzf '" + blockPfad + "' -C '" + zielOrdner + "'";
    return executeTarBefehl(befehl);
}

Result Archiver::extractSingle(const std::string &blockPfad, const std::string &zielOrdner, const std::string &innererPfad) {
    std::error_code ec;
    fs::create_directories(zielOrdner,ec);
    if (ec) return Result::fehler("Zielordner konnte nicht erstellt werden: " + zielOrdner);
    std::string befehl = "tar -xzf '" + blockPfad + "' -C '" + zielOrdner + "' '" + innererPfad + "'";
    return executeTarBefehl(befehl);
}
