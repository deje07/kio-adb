#include <KIO/Global>
#include <KIO/UDSEntry>
#include <KIO/WorkerBase>

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QUrl>

#include <functional>
#include <sys/stat.h>

using KIO::WorkerResult;

Q_LOGGING_CATEGORY(adbLog, "kf.kio.workers.adb", QtWarningMsg)

namespace
{
constexpr int adbTimeout = 30000;

QString quote(QString value)
{
    value.replace("'", "'\\''");
    return "'" + value + "'";
}

QString remotePath(const QUrl &url)
{
    QString path = url.path(QUrl::FullyDecoded);
    if (path.isEmpty()) {
        path = "/";
    }
    if (url.scheme() != "adb" || !url.authority().isEmpty() || !path.startsWith('/') || path.contains(QChar::Null)) {
        return {};
    }
    while (path.size() > 1 && path.endsWith('/')) {
        path.chop(1);
    }
    return path;
}

struct AdbResult {
    bool ok;
    QByteArray output;
    QString error;
    int exitCode = -1;
};

bool succeeded(const QProcess &process)
{
    return process.error() != QProcess::FailedToStart
        && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QString processError(QProcess &process)
{
    const QString message = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (!message.isEmpty()) {
        return message;
    }
    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() != 0) {
        return QStringLiteral("ADB exited with status %1").arg(process.exitCode());
    }
    return process.errorString();
}

AdbResult adb(QStringList arguments, const QString &serial = {},
              const std::function<void(const QByteArray &)> &reportOutput = {})
{
    if (!serial.isEmpty()) {
        arguments.prepend(serial);
        arguments.prepend("-s");
    }
    qCDebug(adbLog) << "Running adb" << arguments;
    QProcess process;
    process.setStandardInputFile(QProcess::nullDevice());
    process.start("adb", arguments);
    if (!process.waitForStarted(adbTimeout)) {
        qCDebug(adbLog) << "Could not start adb:" << process.errorString();
        return {false, {}, process.errorString()};
    }

    // A push can run for much longer than the timeout while it keeps reporting progress.
    while (process.state() != QProcess::NotRunning) {
        if (!process.waitForReadyRead(adbTimeout) && process.state() != QProcess::NotRunning) {
            process.kill();
            process.waitForFinished();
            qCDebug(adbLog) << "ADB command timed out:" << arguments;
            return {false, {}, "ADB command timed out"};
        }
        if (reportOutput) {
            reportOutput(process.readAllStandardOutput());
        }
    }
    const bool ok = succeeded(process);
    const int exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    const auto output = process.readAllStandardOutput();
    const auto error = ok || exitCode == 17 ? QString() : processError(process);
    qCDebug(adbLog) << "ADB finished:" << exitCode << "stdout bytes:" << output.size() << error;
    return {ok, output, error, exitCode};
}

AdbResult shell(const QString &command, const QString &serial = {})
{
    return adb({"shell", "-T", command}, serial);
}

WorkerResult failure(int code, const QString &path, const QString &detail = {})
{
    qCDebug(adbLog) << "Request failed:" << code << path << detail;
    return WorkerResult::fail(code, detail.isEmpty() ? path : path + '\n' + detail);
}

bool parseEntry(const QByteArray &metadata, const QString &name, KIO::UDSEntry &entry)
{
    const auto fields = metadata.trimmed().split(' ');
    if (fields.size() != 3) {
        return false;
    }
    bool modeOk, sizeOk, timeOk;
    const uint mode = fields[0].toUInt(&modeOk, 16);
    const auto size = fields[1].toLongLong(&sizeOk);
    const auto modified = fields[2].toLongLong(&timeOk);
    if (!modeOk || !sizeOk || !timeOk) {
        return false;
    }
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, name);
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, mode & S_IFMT);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, mode & 0777);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, size);
    entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, modified);
    return true;
}

// Follow links when possible so Android's shared storage aliases can be browsed.
const QString statCommand = "stat -L -c '%f %s %Y' ";
const QString lstatCommand = "stat -c '%f %s %Y' ";

class AdbWorker final : public KIO::WorkerBase
{
public:
    AdbWorker(const QByteArray &pool, const QByteArray &app)
        : WorkerBase("adb", pool, app)
    {
    }

    WorkerResult listDir(const QUrl &url) override
    {
        qCDebug(adbLog) << "listDir" << url.path();
        if (url.path().isEmpty() && url.authority().isEmpty()) {
            QUrl root = url;
            root.setPath("/");
            redirection(root);
            return WorkerResult::pass();
        }
        const QString path = remotePath(url);
        if (path.isEmpty()) {
            return failure(KIO::ERR_MALFORMED_URL, url.toDisplayString());
        }

        // NUL-delimited names allow whitespace and newlines in filenames.
        const auto result = shell("cd " + quote(path) + " && "
            "test -r . && test -x . && "
            "for file in . ./* ./.[!.]* ./..?*; do "
            "metadata=$(" + statCommand + "\"$file\" 2>/dev/null || "
            + lstatCommand + "\"$file\" 2>/dev/null) || continue; "
            "printf '%s\\000%s\\000' \"${file#./}\" \"$metadata\"; done");
        if (!result.ok) {
            return failure(KIO::ERR_CANNOT_ENTER_DIRECTORY, path, result.error);
        }
        const auto records = result.output.split('\0');
        if (records.size() % 2 != 1 || !records.last().isEmpty()) {
            return failure(KIO::ERR_CANNOT_ENTER_DIRECTORY, path, "Invalid ADB directory listing");
        }
        for (qsizetype index = 0; index + 1 < records.size(); index += 2) {
            const QString name = QString::fromUtf8(records[index]);
            KIO::UDSEntry entry;
            if (name.toUtf8() == records[index] && parseEntry(records[index + 1], name, entry)) {
                listEntry(entry);
            }
        }
        return WorkerResult::pass();
    }

    WorkerResult stat(const QUrl &url) override
    {
        qCDebug(adbLog) << "stat" << url.path();
        const QString path = remotePath(url);
        if (path.isEmpty()) {
            return failure(KIO::ERR_MALFORMED_URL, url.toDisplayString());
        }
        const QString argument = quote(path);
        const auto result = shell(statCommand + argument + " 2>/dev/null || " + lstatCommand + argument);
        KIO::UDSEntry entry;
        if (!result.ok || !parseEntry(result.output, path == "/" ? "." : path.section('/', -1), entry)) {
            return failure(KIO::ERR_DOES_NOT_EXIST, path, result.error);
        }
        statEntry(entry);
        return WorkerResult::pass();
    }

    WorkerResult get(const QUrl &url) override
    {
        qCDebug(adbLog) << "get" << url.path();
        const QString path = remotePath(url);
        if (path.isEmpty()) {
            return failure(KIO::ERR_MALFORMED_URL, url.toDisplayString());
        }
        const QString argument = quote(path);
        QProcess process;
        process.setStandardInputFile(QProcess::nullDevice());
        process.start("adb", {"shell", "-T", "test -f " + argument + " && cat " + argument});
        if (!process.waitForStarted(adbTimeout)) {
            return failure(KIO::ERR_CANNOT_READ, path, process.errorString());
        }

        bool sentMimeType = false;
        KIO::filesize_t received = 0;
        while (true) {
            while (process.bytesAvailable() > 0) {
                const QByteArray chunk = process.read(64 * 1024);
                if (!sentMimeType) {
                    mimeType(QMimeDatabase().mimeTypeForFileNameAndData(path, chunk).name());
                    sentMimeType = true;
                }
                data(chunk);
                received += chunk.size();
                processedSize(received);
            }
            if (process.state() == QProcess::NotRunning) {
                break;
            }
            if (!process.waitForReadyRead(adbTimeout) && process.state() != QProcess::NotRunning) {
                process.kill();
                process.waitForFinished();
                return failure(KIO::ERR_CANNOT_READ, path, "ADB download timed out");
            }
        }
        if (!succeeded(process)) {
            return failure(KIO::ERR_CANNOT_READ, path, processError(process));
        }
        if (!sentMimeType) {
            mimeType(QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchExtension).name());
        }
        data({});
        qCDebug(adbLog) << "Download complete:" << path << received << "bytes";
        return WorkerResult::pass();
    }

    WorkerResult copy(const QUrl &from, const QUrl &to, int permissions, KIO::JobFlags flags) override
    {
        if (!from.isLocalFile()) {
            return WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, from.toDisplayString());
        }
        const QString source = from.toLocalFile();
        const QFileInfo info(source);
        if (!info.isFile() || !info.isReadable()) {
            return failure(KIO::ERR_CANNOT_READ, source);
        }
        return upload(to, permissions, flags, info.canonicalFilePath(), info.size());
    }

    WorkerResult put(const QUrl &url, int permissions, KIO::JobFlags flags) override
    {
        qCDebug(adbLog) << "put" << url.path();
        const QString path = remotePath(url);
        if (path.isEmpty() || path == "/") {
            return failure(KIO::ERR_MALFORMED_URL, url.toDisplayString());
        }
        if (flags & KIO::Resume) {
            return failure(KIO::ERR_UNSUPPORTED_ACTION, url.toDisplayString());
        }
        canResume(0);
        QTemporaryFile local;
        if (!local.open()) {
            return failure(KIO::ERR_CANNOT_WRITE, path, local.errorString());
        }
        while (true) {
            QByteArray chunk;
            dataReq();
            const auto count = readData(chunk);
            if (count < 0 || local.write(chunk) != count) {
                return failure(KIO::ERR_CANNOT_WRITE, path, local.errorString());
            }
            if (count == 0) {
                break;
            }
        }
        if (!local.flush()) {
            return failure(KIO::ERR_CANNOT_WRITE, path, local.errorString());
        }
        local.close();

        return upload(url, permissions, flags, local.fileName(), local.size());
    }

    WorkerResult upload(const QUrl &url, int permissions, KIO::JobFlags flags,
                        const QString &source, KIO::filesize_t size)
    {
        const QString path = remotePath(url);
        if (path.isEmpty() || path == "/") {
            return failure(KIO::ERR_MALFORMED_URL, url.toDisplayString());
        }
        if (flags & KIO::Resume) {
            return failure(KIO::ERR_UNSUPPORTED_ACTION, path);
        }
        const auto device = adb({"get-serialno"});
        const QString serial = QString::fromUtf8(device.output).trimmed();
        if (!device.ok || serial.isEmpty() || serial == "unknown") {
            return failure(KIO::ERR_CANNOT_CONNECT, path, device.error);
        }
        const QString target = quote(path);
        const auto exists = shell("test -e " + target + " || test -L " + target, serial);
        const bool overwrite = flags.testFlag(KIO::Overwrite);
        if (exists.ok && !overwrite) {
            return failure(KIO::ERR_FILE_ALREADY_EXIST, path);
        }

        const auto destination = shell(
            "if [ -L " + target + " ] || { [ -e " + target + " ] && [ ! -f " + target + " ]; }; then exit 18; fi; "
            "if [ -f " + target + " ]; then stat -c '%a' " + target + "; fi", serial);
        if (!destination.ok) {
            return failure(KIO::ERR_CANNOT_WRITE, path, destination.error);
        }
        if (!destination.output.isEmpty() && !overwrite) {
            return failure(KIO::ERR_FILE_ALREADY_EXIST, path);
        }

        totalSize(size);
        static const QRegularExpression progressLine(QStringLiteral(R"(^\[ *(\d{1,3})%\])"));
        QByteArray pending;
        int lastPercent = 0;
        const auto pushed = adb({"push", source, path}, serial, [&](const QByteArray &output) {
            pending += output;
            pending.replace('\r', '\n');
            qsizetype end;
            while ((end = pending.indexOf('\n')) >= 0) {
                const auto match = progressLine.match(QString::fromUtf8(pending.first(end)));
                pending.remove(0, end + 1);
                const int percent = match.captured(1).toInt();
                if (percent > lastPercent && percent < 100) {
                    lastPercent = percent;
                    processedSize(size / 100 * percent + size % 100 * percent / 100);
                }
            }
        });
        if (!pushed.ok) {
            return failure(KIO::ERR_CANNOT_WRITE, path, pushed.error);
        }

        bool modeOk = false;
        const int existingMode = destination.output.trimmed().toInt(&modeOk, 8);
        const int mode = modeOk ? existingMode : permissions;
        const auto modified = QDateTime::fromString(metaData("modified"), Qt::ISODate);
        if (mode >= 0) {
            shell("chmod " + QString::number(mode & 0777, 8) + ' ' + target, serial);
        }
        if (modified.isValid()) {
            shell("touch -m -d @" + QString::number(modified.toSecsSinceEpoch()) + ' ' + target, serial);
        }
        processedSize(size);
        return WorkerResult::pass();
    }

    WorkerResult mkdir(const QUrl &url, int permissions) override
    {
        qCDebug(adbLog) << "mkdir" << url.path();
        const QString path = remotePath(url);
        if (path.isEmpty()) {
            return failure(KIO::ERR_MALFORMED_URL, url.toDisplayString());
        }
        const QString target = quote(path);
        const QString mode = permissions < 0 ? QString() : "-m " + QString::number(permissions & 0777, 8) + ' ';
        // Let mkdir decide whether creation succeeded, including concurrent creation.
        const auto result = shell("mkdir " + mode + target
            + " || { if [ -d " + target + " ]; then exit 19; fi; "
              "if [ -e " + target + " ] || [ -L " + target + " ]; then exit 17; fi; exit 1; }");
        if (result.exitCode == 19) {
            return failure(KIO::ERR_DIR_ALREADY_EXIST, path);
        }
        return moveResult(result, path, KIO::ERR_CANNOT_MKDIR);
    }

    WorkerResult rename(const QUrl &from, const QUrl &to, KIO::JobFlags flags) override
    {
        qCDebug(adbLog) << "rename" << from.path() << "to" << to.path();
        const QString source = remotePath(from);
        const QString target = remotePath(to);
        if (source.isEmpty() || target.isEmpty() || source == "/" || target == "/") {
            return failure(KIO::ERR_MALFORMED_URL, from.toDisplayString());
        }
        const auto result = shell(moveCommand(quote(source), quote(target), flags.testFlag(KIO::Overwrite)));
        return moveResult(result, target, KIO::ERR_CANNOT_RENAME);
    }

    WorkerResult del(const QUrl &url, bool isFile) override
    {
        qCDebug(adbLog) << "del" << url.path();
        const QString path = remotePath(url);
        if (path.isEmpty() || path == "/") {
            return failure(KIO::ERR_MALFORMED_URL, url.toDisplayString());
        }
        // A directory symlink can be advertised as a directory, but must be unlinked.
        const QString target = quote(path);
        const auto result = shell(isFile ? "rm " + target :
            "if [ -L " + target + " ]; then rm " + target + "; else rmdir " + target + "; fi");
        return result.ok ? WorkerResult::pass() : failure(KIO::ERR_CANNOT_DELETE, path, result.error);
    }

private:
    static QString moveCommand(const QString &source, const QString &target, bool overwrite)
    {
        if (overwrite) {
            return "mv -fT " + source + ' ' + target;
        }
        // Toybox mv -n can report success when it skips a collision.
        return "if [ -e " + target + " ] || [ -L " + target + " ]; then exit 17; fi; "
            "mv -nT " + source + ' ' + target + " && "
            "if [ -e " + source + " ] || [ -L " + source + " ]; then exit 17; fi";
    }

    static WorkerResult moveResult(const AdbResult &result, const QString &path, int error)
    {
        return result.ok ? WorkerResult::pass()
            : failure(result.exitCode == 17 ? KIO::ERR_FILE_ALREADY_EXIST : error, path, result.error);
    }
};

class AdbPlugin : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.adb" FILE "adb.json")
};
}

extern "C" Q_DECL_EXPORT int kdemain(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (argc != 4) {
        return 1;
    }
    AdbWorker worker(argv[2], argv[3]);
    worker.dispatchLoop();
    return 0;
}

#include "adbworker.moc"
