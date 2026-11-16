#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KIO/SimpleJob>
#include <KIO/ListJob>
#include <KIO/MkdirJob>

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QMap>

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
    const QUrl remote = QUrl::fromEncoded(directory.toEncoded() + QUrl::toPercentEncoding(name));

    const auto copy = [](const QUrl &from, const QUrl &to) {
        auto *job = KIO::copyAs(from, to, KIO::HideProgressInfo);
        if (!job->exec()) {
            qCritical() << from << "->" << to << job->errorString();
            return 1;
        }
        return 0;
    };
    if (action == "upload-multiple") {
        const QList<QUrl> sources{QUrl::fromLocalFile(arguments[3]), QUrl::fromLocalFile(arguments[4])};
        const auto firstSize = QFileInfo(arguments[3]).size();
        const auto secondSize = QFileInfo(arguments[4]).size();
        auto *job = KIO::copy(sources, directory, KIO::HideProgressInfo);
        bool sawPartialFile = false;
        qulonglong processedBytes = 0;
        QObject::connect(job, &KJob::processedSize, &application,
                         [&](KJob *, qulonglong bytes) {
            sawPartialFile |= bytes > 0 && bytes < qMin(firstSize, secondSize);
            processedBytes = bytes;
        });
        if (!job->exec()) {
            qCritical() << "Multi-file upload failed:" << job->errorString();
            return 1;
        }
        if (!sawPartialFile || processedBytes != qulonglong(firstSize + secondSize)) {
            qCritical() << "Expected partial-file progress and a final byte count, got"
                        << sawPartialFile << processedBytes;
            return 1;
        }
        return 0;
    }
    if (action == "delete-multiple") {
        const auto second = QUrl::fromEncoded(directory.toEncoded() + QUrl::toPercentEncoding(arguments[3]));
        auto *job = KIO::del(QList<QUrl>{remote, second}, KIO::HideProgressInfo);
        bool sawPartialProgress = false;
        unsigned long finalPercent = 0;
        QObject::connect(job, &KJob::percentChanged, &application,
                         [&](KJob *, unsigned long percent) {
            sawPartialProgress |= percent == 50;
            finalPercent = percent;
        });
        if (!job->exec()) {
            qCritical() << "Multi-file deletion failed:" << job->errorString();
            return 1;
        }
        if (!sawPartialProgress || finalPercent != 100) {
            qCritical() << "Expected deletion progress at 50% and 100%, got"
                        << sawPartialProgress << finalPercent;
            return 1;
        }
        return 0;
    }
    if (action == "download") {
        return copy(remote, QUrl::fromLocalFile(arguments[4]));
    }
    if (action == "upload" || action == "upload-space") {
        return copy(QUrl::fromLocalFile(arguments[3]), remote);
    }
    if (action == "rmdir") {
        auto *job = KIO::rmdir(remote);
        if (!job->exec()) {
            qCritical() << "Directory deletion failed:" << remote << job->errorString();
            return 1;
        }
        return 0;
    }
    if (action == "mkdir") {
        auto *job = KIO::mkdir(remote);
        if (!job->exec()) {
            qCritical() << "Directory creation failed:" << remote << job->errorString();
            return 1;
        }
        auto *duplicate = KIO::mkdir(remote);
        if (duplicate->exec() || duplicate->error() != KIO::ERR_DIR_ALREADY_EXIST) {
            qCritical() << "Expected a directory collision:" << remote << duplicate->errorString();
            return 1;
        }
        return 0;
    }
    if (action == "list-directory") {
        QMap<QString, qlonglong> remaining{
            {QStringLiteral("alpha.txt"), 5},
            {QStringLiteral("two words.txt"), 10},
            {QStringLiteral("empty.txt"), 0},
            {QStringLiteral(".hidden.txt"), 6},
        };
        bool matches = true;
        auto *listing = KIO::listDir(remote, KIO::HideProgressInfo);
        QObject::connect(listing, &KIO::ListJob::entries, &application,
                         [&](KIO::Job *, const KIO::UDSEntryList &entries) {
            for (const auto &entry : entries) {
                const QString filename = entry.stringValue(KIO::UDSEntry::UDS_NAME);
                if (filename == "." || filename == "..") {
                    continue;
                }
                const auto expected = remaining.find(filename);
                if (expected == remaining.end()) {
                    qCritical() << "Unexpected or duplicate entry:" << filename;
                    matches = false;
                    continue;
                }
                if (entry.contains(KIO::UDSEntry::UDS_SIZE)
                    && entry.numberValue(KIO::UDSEntry::UDS_SIZE) != expected.value()) {
                    qCritical() << "Wrong size for" << filename << "expected" << expected.value()
                                << "got" << entry.numberValue(KIO::UDSEntry::UDS_SIZE);
                    matches = false;
                }
                remaining.erase(expected);
            }
        });
        if (!listing->exec()) {
            qCritical() << "Directory listing failed:" << remote << listing->errorString();
            return 1;
        }
        if (!remaining.isEmpty()) {
            qCritical() << "Missing entries:" << remaining.keys();
            return 1;
        }
        return matches ? 0 : 1;
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
