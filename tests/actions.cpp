#include <KIO/CopyJob>
#include <KIO/ListJob>

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() != 5) {
        qCritical() << "Expected action, remote filename, fixture path, and download path";
        return 1;
    }
    const QString action = arguments[1];
    const QString name = arguments[2];
    const QUrl directory("adb:/sdcard/");
    const QUrl remote = directory.resolved(QUrl(name));

    const auto copy = [](const QUrl &from, const QUrl &to) {
        auto *job = KIO::copyAs(from, to, KIO::HideProgressInfo);
        if (!job->exec()) {
            qCritical() << from << "->" << to << job->errorString();
            return 1;
        }
        return 0;
    };
    if (action == "download") {
        return copy(remote, QUrl::fromLocalFile(arguments[4]));
    }
    if (action == "upload") {
        return copy(QUrl::fromLocalFile(arguments[3]), remote);
    }
    if (action != "list") {
        qCritical() << "Unknown test action:" << action;
        return 1;
    }

    bool found = false;
    auto *listing = KIO::listDir(directory, KIO::HideProgressInfo);
    QObject::connect(listing, &KIO::ListJob::entries, &application,
                     [&](KIO::Job *, const KIO::UDSEntryList &entries) {
        for (const auto &entry : entries) {
            if (entry.stringValue(KIO::UDSEntry::UDS_NAME) == name) {
                found = true;
            }
        }
    });
    if (!listing->exec() || !found) {
        qCritical() << "Fixture missing from the SD card listing:" << name << listing->errorString();
        return 1;
    }
    return 0;
}
