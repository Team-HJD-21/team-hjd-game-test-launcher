#include "core/Core.h"
#include "core/CloudflareLogin.h"
#include "miniz.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QtEndian>
#include <Security/Security.h>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

using namespace hjd;
namespace {
int passed = 0, failed = 0;
void check(bool condition, const QString &name) {
    QTextStream(stdout) << (condition ? "PASS " : "FAIL ") << name << '\n';
    if(condition) ++passed; else ++failed;
}
void rejects(const QString &name, const std::function<void()> &body) {
    try { body(); check(false, name); } catch(const Error &) { check(true, name); }
}
template<class T> class OwnedCF {
public:
    explicit OwnedCF(T value = nullptr): m_value(value) {}
    ~OwnedCF() { if(m_value) CFRelease(m_value); }
    OwnedCF(const OwnedCF &) = delete;
    OwnedCF &operator=(const OwnedCF &) = delete;
    T get() const { return m_value; }
private:
    T m_value;
};

QByteArray rawSignature(const QByteArray &der) {
    // Security returns a short-form X9.62 DER sequence for a 256-bit curve.
    // Decode independently from the product's raw -> DER implementation.
    int offset = 0;
    auto byte = [&]() -> unsigned char {
        if(offset >= der.size()) fail("fixture signature DER truncated");
        return static_cast<unsigned char>(der[offset++]);
    };
    if(byte() != 0x30 || byte() != der.size() - 2) fail("fixture signature sequence");
    QByteArray result;
    for(int i = 0; i != 2; ++i) {
        if(byte() != 2) fail("fixture signature integer");
        const int size = byte();
        if(size < 1 || size > 33 || offset + size > der.size()) fail("fixture signature size");
        auto integer = der.mid(offset, size); offset += size;
        if(integer.size() == 33) {
            if(integer[0] != 0) fail("fixture integer overflow");
            integer.remove(0, 1);
        }
        result += QByteArray(32 - integer.size(), '\0') + integer;
    }
    if(offset != der.size()) fail("fixture signature trailing bytes");
    return result;
}
struct SigningKey {
    OwnedCF<SecKeyRef> key;
    QString publicKey;
    SigningKey(): key(create()) {
        OwnedCF<SecKeyRef> pub(SecKeyCopyPublicKey(key.get()));
        CFErrorRef error = nullptr;
        OwnedCF<CFDataRef> exported(SecKeyCopyExternalRepresentation(pub.get(), &error));
        OwnedCF<CFErrorRef> ownedError(error);
        if(!exported.get() || CFDataGetLength(exported.get()) != 65 || CFDataGetBytePtr(exported.get())[0] != 4)
            fail("fixture public key export");
        QByteArray point(reinterpret_cast<const char *>(CFDataGetBytePtr(exported.get()) + 1), 64);
        publicKey = "p256:" + QString::fromLatin1(point.toBase64());
    }
    static SecKeyRef create() {
        int bits = 256;
        OwnedCF<CFNumberRef> number(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bits));
        const void *keys[] = {kSecAttrKeyType, kSecAttrKeySizeInBits};
        const void *values[] = {kSecAttrKeyTypeECSECPrimeRandom, number.get()};
        OwnedCF<CFDictionaryRef> attrs(CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2,
                                                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFErrorRef error = nullptr;
        auto key = SecKeyCreateRandomKey(attrs.get(), &error);
        OwnedCF<CFErrorRef> ownedError(error);
        if(!key) fail("fixture signing key creation");
        return key;
    }
    QByteArray sign(const QByteArray &payload) const {
        OwnedCF<CFDataRef> data(CFDataCreate(kCFAllocatorDefault,
            reinterpret_cast<const UInt8 *>(payload.constData()), payload.size()));
        CFErrorRef error = nullptr;
        OwnedCF<CFDataRef> der(SecKeyCreateSignature(key.get(), kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
                                                   data.get(), &error));
        OwnedCF<CFErrorRef> ownedError(error);
        if(!der.get()) fail("fixture signature creation");
        return rawSignature(QByteArray(reinterpret_cast<const char *>(CFDataGetBytePtr(der.get())), CFDataGetLength(der.get())));
    }
    QByteArray envelope(const QJsonObject &object) const {
        const auto payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
        return QJsonDocument(QJsonObject{{"payload", QString::fromLatin1(payload.toBase64())},
            {"signature", QString::fromLatin1(sign(payload).toBase64())}}).toJson(QJsonDocument::Compact);
    }
};

struct ZipEntry { QString path; QByteArray bytes; quint32 mode = 0100644; };
void makeZip(const QString &path, const QList<ZipEntry> &entries) {
    FILE *file = std::fopen(QFile::encodeName(path).constData(), "wb+");
    if(!file) fail("fixture ZIP open");
    mz_zip_archive zip{};
    if(!mz_zip_writer_init_cfile(&zip, file, 0)) { fclose(file); fail("fixture ZIP init"); }
    bool ok = true;
    for(const auto &entry: entries) {
        const auto name = entry.path.toUtf8();
        if(!mz_zip_writer_add_mem(&zip, name.constData(), entry.bytes.constData(), size_t(entry.bytes.size()), MZ_BEST_SPEED)) ok = false;
    }
    ok = mz_zip_writer_finalize_archive(&zip) && ok;
    mz_zip_writer_end(&zip); fclose(file);
    if(!ok) fail("fixture ZIP write");

    // miniz's memory writer has no Unix mode argument. Set central-directory
    // metadata to the same standard mode field used by zipfile/ditto.
    auto bytes = readFile(path);
    int offset = bytes.lastIndexOf(QByteArray::fromHex("504b0506"));
    if(offset < 0 || offset + 22 > bytes.size()) fail("fixture ZIP end record");
    offset = int(qFromLittleEndian<quint32>(bytes.constData() + offset + 16));
    for(const auto &entry: entries) {
        if(offset < 0 || offset + 46 > bytes.size() || bytes.mid(offset, 4) != QByteArray::fromHex("504b0102")) fail("fixture ZIP central record");
        const auto nameSize = qFromLittleEndian<quint16>(bytes.constData() + offset + 28);
        const auto extraSize = qFromLittleEndian<quint16>(bytes.constData() + offset + 30);
        const auto commentSize = qFromLittleEndian<quint16>(bytes.constData() + offset + 32);
        if(bytes.mid(offset + 46, nameSize) != entry.path.toUtf8()) fail("fixture ZIP central name");
        qToLittleEndian<quint16>(quint16(3 * 256 + 20), bytes.data() + offset + 4);
        qToLittleEndian<quint32>(entry.mode << 16, bytes.data() + offset + 38);
        offset += 46 + nameSize + extraSize + commentSize;
    }
    atomicWrite(path, bytes);
}
QString sha256(const QByteArray &bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QList<ZipEntry> fixtureEntries() {
    return {{"Fixture.app/Contents/MacOS/Fixture", readFile(QCoreApplication::applicationDirPath() + "/LauncherFixtureGame"), 0100755},
            {"Fixture.app/Contents/Info.plist", QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
            "<plist version=\"1.0\"><dict><key>CFBundleExecutable</key><string>Fixture</string>"
            "<key>CFBundleIdentifier</key><string>com.teamhjd.launcher.test.fixture</string>"
            "<key>CFBundleName</key><string>Fixture</string><key>CFBundlePackageType</key><string>APPL</string></dict></plist>"), 0100644}};
}
Game fixture(const QString &archive, const QList<ZipEntry> &entries) {
    Game game; game.id = "fixture"; game.name = "Mac Fixture"; game.version = "1.0.0";
    game.executable = "Fixture.app/Contents/MacOS/Fixture"; game.platform = "macos-universal";
    game.packageUrl = QUrl("https://localhost/packages/fixture.zip"); game.sha256 = fileHash(archive);
    game.bytes = QFileInfo(archive).size();
    for(const auto &entry: entries) {
        game.files.append({entry.path, sha256(entry.bytes), entry.bytes.size()});
        game.expandedBytes += entry.bytes.size();
    }
    return game;
}
QJsonObject catalog(const Game &game) {
    return {{"schema", 1}, {"sequence", 5}, {"issuedAt", QDateTime::currentDateTimeUtc().addSecs(-60).toString(Qt::ISODate)},
            {"expiresAt", QDateTime::currentDateTimeUtc().addDays(7).toString(Qt::ISODate)}, {"games", QJsonArray{game.toJson()}}};
}
quint32 modeOf(const QString &path) {
    struct stat info{};
    if(stat(QFile::encodeName(path).constData(), &info)) fail("fixture stat");
    return quint32(info.st_mode);
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        if(app.arguments().contains("--verify-payload")) {
            const auto args = app.arguments();
            if(args.size() != 4) fail("--verify-payload archive destination");
            std::atomic_bool cancel = false;
            Store store(args[3]);
            auto folder = store.stagePayload(QStringLiteral(HJD_VERSION), fileHash(args[2]), args[2], cancel);
            store.activate("launcher", folder);
            if(!store.verify("launcher")) fail("release payload tree mismatch");
            QTextStream(stdout) << "PASS complete macOS release payload extraction, permissions and verification\n";
            return 0;
        }
        QDir().mkpath(QDir::homePath() + "/.teamhjd-ci-tests");
        QTemporaryDir temp(QDir::homePath() + "/.teamhjd-ci-tests/core-한글-XXXXXX");
        if(!temp.isValid()) fail("test directory");
        const auto root = temp.path();

        for(const auto &path: QStringList{"../evil", "/root", "a\\b", "a/../x", "file.", "file ",
                                        "NUL.txt", "COM1.exe", "a:b", "a//b", "._teamhjd-receipt.json", "a/<b"})
            check(!safePath(path), "reject path " + path);
        check(safePath("게임.app/Contents/설정 파일.json"), "Unicode bundle paths accepted");
        check(!allowedServer(QUrl("http://example.com"), true), "Internet HTTP rejected even in test mode");
        check(!sameOrigin(QUrl("https://a:8443"), QUrl("https://a:8444")), "origin includes port");
        check(!allowedServer(QUrl("https://user:pw@example.com")), "URL credentials rejected");
        atomicWrite(root + "/plain/target", "fixture");
        if(::symlink(QFile::encodeName(root + "/plain").constData(), QFile::encodeName(root + "/link").constData())) fail("fixture symlink");
        rejects("symlink ancestor refused", [&]{ noLinks(root + "/link/target"); });
        rejects("nonexistent descendant beneath symlink refused", [&]{ noLinks(root + "/link/new/file"); });
        if(!QFile::remove(root + "/link")) fail("fixture unlink");
        noLinks(root + "/plain/target"); check(true, "ordinary directory tree accepted");

        // RFC 6979 A.2.5 P-256 SHA-256, message "sample". This is an external
        // interoperability vector: https://www.rfc-editor.org/rfc/rfc6979#appendix-A.2.5
        const auto point = QByteArray::fromHex("60FED4BA255A9D31C961EB74C6356D68C049B8923B61FA6CE669622E60F29FB6"
                                               "7903FE1008B8BC99A41AE9E95628BC64F2F1B20C2D7E9F5177A3C294D4462299");
        const auto signature = QByteArray::fromHex("EFD48B2AACB6A8FD1140DD9CD45E81D69D2C877B56AAF991C34D0EA84EAF3716"
                                                   "F7CB1C942D657C41D436C7A1B6E29F65F3E900DBB9AFF4064DC4AB2F843ACDA8");
        const auto publicKey = "p256:" + QString::fromLatin1(point.toBase64());
        check(verifySignature("sample", signature, publicKey), "RFC6979 P256 raw signature accepted by Apple verifier");
        check(!verifySignature("Sample", signature, publicKey), "altered RFC payload rejected");
        check(!verifySignature("sample", signature.left(63), publicKey), "short raw signature rejected");
        check(!verifySignature("sample", QByteArray(64, '\0'), publicKey), "zero signature rejected");
        check(!verifySignature("sample", signature, "p256:AAAA"), "malformed publisher point rejected");

        if(qgetenv("TEAMHJD_TEST_KEYCHAIN_READY") != "1")
            fail("Create and unlock a disposable CI keychain before setting TEAMHJD_TEST_KEYCHAIN_READY=1");
        const QByteArray secret("fixture-only-한국어");
        auto encrypted = protect(secret);
        check(encrypted != secret && !encrypted.contains(secret) && unprotect(encrypted) == secret,
              "Keychain-wrapped authenticated secret round trip");
        auto again = protect(secret);
        check(encrypted != again && unprotect(again) == secret, "fresh nonce for repeated credential saves");
        auto altered = encrypted; altered[altered.size() / 2] = char(altered[altered.size() / 2] ^ 1);
        rejects("altered credential ciphertext rejected", [&]{ unprotect(altered); });
        rejects("truncated credential rejected", [&]{ unprotect(encrypted.left(encrypted.size() - 1)); });
        check(unprotect(protect({})).isEmpty(), "empty credentials round trip");

        auto entries = fixtureEntries(); const auto zip = root + "/fixture.zip"; makeZip(zip, entries);
        auto game = fixture(zip, entries); std::atomic_bool cancel = false;
        check(Game::parse(game.toJson(), QUrl("https://localhost/")).supported(), "macOS universal game recognized");
        auto windows = game.toJson(); windows.remove("platform"); windows["executable"] = "Fixture.exe";
        windows["files"] = QJsonArray{QJsonObject{{"path", "Fixture.exe"}, {"sha256", sha256(entries[0].bytes)}, {"bytes", entries[0].bytes.size()}}};
        windows["expandedBytes"] = entries[0].bytes.size();
        auto windowsGame = Game::parse(windows, QUrl("https://localhost/"));
        check(windowsGame.platform == "windows-x64" && !windowsGame.supported(), "legacy manifest remains Windows-only on Mac");
        auto wrongPath = game.toJson(); wrongPath["executable"] = "Fixture.exe";
        rejects("Mac metadata cannot select Windows exe", [&]{ Game::parse(wrongPath, QUrl("https://localhost/")); });
        auto unknown = game.toJson(); unknown["platform"] = "unknown";
        rejects("unknown platform rejected", [&]{ Game::parse(unknown, QUrl("https://localhost/")); });
        auto remote = game.toJson(); remote["url"] = "https://evil.invalid/a.zip";
        rejects("cross-origin package rejected", [&]{ Game::parse(remote, QUrl("https://localhost/")); });

        SigningKey key; const auto cat = catalog(game); const auto envelope = key.envelope(cat);
        auto verified = verifyCatalog(envelope, key.publicKey, QUrl("https://localhost/"), 5);
        check(verified.games.size() == 1 && verified.sequence == 5, "Apple P256 signed catalog accepted");
        auto native = game;
#ifdef Q_PROCESSOR_ARM_64
        native.platform = "macos-arm64";
        const QString foreignPlatform = "macos-x64";
#else
        native.platform = "macos-x64";
        const QString foreignPlatform = "macos-arm64";
#endif
        native.version = "1.2.0";
        auto foreign = game; foreign.platform = foreignPlatform; foreign.version = "1.3.0";
        auto platformBase = windowsGame.toJson();
        platformBase["platforms"] = QJsonObject{{"macos-universal", game.toJson()},
            {native.platform, native.toJson()}, {foreign.platform, foreign.toJson()}};
        auto platformCatalog = cat; platformCatalog["games"] = QJsonArray{platformBase};
        const auto nativeCatalog = verifyCatalog(key.envelope(platformCatalog), key.publicKey, QUrl("https://localhost/"));
        check(nativeCatalog.games.first().platform == native.platform && nativeCatalog.games.first().version == "1.2.0",
              "signed native architecture preferred over universal and incompatible Mac package");
        platformBase["platforms"] = QJsonObject{{"macos-universal", game.toJson()}, {foreign.platform, foreign.toJson()}};
        platformCatalog["games"] = QJsonArray{platformBase};
        check(verifyCatalog(key.envelope(platformCatalog), key.publicKey, QUrl("https://localhost/")).games.first().platform == "macos-universal",
              "universal package selected when native architecture is absent");
        platformBase["platforms"] = QJsonObject{{foreign.platform, foreign.toJson()}};
        platformCatalog["games"] = QJsonArray{platformBase};
        check(!verifyCatalog(key.envelope(platformCatalog), key.publicKey, QUrl("https://localhost/")).games.first().supported(),
              "incompatible Mac architecture never replaces Windows fallback");
        auto mismatched = game.toJson(); mismatched["id"] = "other-game";
        platformBase["platforms"] = QJsonObject{{"macos-universal", mismatched}};
        platformCatalog["games"] = QJsonArray{platformBase};
        rejects("nested platform cannot replace another game identity", [&]{ verifyCatalog(key.envelope(platformCatalog), key.publicKey, QUrl("https://localhost/")); });
        rejects("catalog rollback rejected", [&]{ verifyCatalog(envelope, key.publicKey, QUrl("https://localhost/"), 6); });
        rejects("wrong signing key rejected", [&]{ SigningKey other; verifyCatalog(envelope, other.publicKey, QUrl("https://localhost/")); });
        auto tampered = QJsonDocument::fromJson(envelope).object(); auto changed = cat; changed["sequence"] = 99;
        tampered["payload"] = QString::fromLatin1(QJsonDocument(changed).toJson(QJsonDocument::Compact).toBase64());
        rejects("catalog tamper rejected", [&]{ verifyCatalog(QJsonDocument(tampered).toJson(), key.publicKey, QUrl("https://localhost/")); });

        Store store(root + "/games");
        rejects("Windows package installation blocked on Mac", [&]{ store.install(windowsGame, zip, cancel); });
        store.install(game, zip, cancel); check(store.verify(game.id), "Mac game first install fully verified");
        const auto original = store.currentDirectory(game.id); const auto originalState = store.state(game.id);
        check((modeOf(original + '/' + game.executable) & 0111) != 0, "native game executable bit retained");
        check((modeOf(original + "/Fixture.app/Contents/Info.plist") & 0111) == 0, "ordinary bundle data remains non-executable");
        store.install(game, zip, cancel);
        check(store.state(game.id) == originalState && QDir(root + "/games/fixture/versions").entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() == 1,
              "identical game does not create another installation");
        auto two = game; two.version = "1.1.0"; store.install(two, zip, cancel);
        check(store.currentReceipt(game.id)["version"] == "1.1.0" && QFileInfo::exists(original + '/' + game.executable), "update retains previous verified installation");
        store.rollback(game.id); check(store.currentDirectory(game.id) == original, "rollback restores original installation");
        auto stable = store.state(game.id); auto bad = game; bad.sha256 = QString(64, '0');
        rejects("incorrect archive digest rejected", [&]{ store.install(bad, zip, cancel); });
        check(store.state(game.id) == stable, "digest failure preserves active game");
        const auto launched = store.launch(game.id);
        check(store.running(game.id) && processStamp(launched, original + '/' + game.executable) != 0, "native process identity matches verified executable");
        check(processStamp(launched, root + "/wrong-executable") == 0, "process identity rejects wrong executable path");
        rejects("running native game prevents update", [&]{ store.install(two, zip, cancel); });
        rejects("running native game prevents uninstall", [&]{ store.uninstall(game.id); });
        QThread::msleep(3500); check(!store.running(game.id), "native game exit detected");

        const auto exec = original + '/' + game.executable; const auto permissions = QFileInfo(exec).permissions();
        QFile::setPermissions(exec, permissions & ~(QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther));
        check(!store.verify(game.id), "removed executable bit invalidates receipt verification");
        rejects("game with changed executable mode cannot launch", [&]{ store.launch(game.id); });
        QFile::setPermissions(exec, permissions);
        check(store.verify(game.id), "restoring expected executable mode repairs tree verification");

        int unsafe = 0;
        for(const auto &path: QStringList{"../escape", "a\\escape", "NUL.exe", "foo.", "._teamhjd-receipt.json"}) {
            const auto archive = root + "/bad-" + QString::number(unsafe) + ".zip";
            makeZip(archive, {{path, "fixture", 0100644}});
            rejects("ZIP unsafe path " + path, [&]{ inspectArchive(archive, root + "/unpack-" + QString::number(unsafe++), {}, cancel); });
        }
        makeZip(root + "/case.zip", {{"A", "a"}, {"a", "b"}});
        rejects("case-colliding ZIP paths rejected on Mac", [&]{ inspectArchive(root + "/case.zip", root + "/case", {}, cancel); });
        makeZip(root + "/collision.zip", {{"data", "a"}, {"data/file", "b"}});
        rejects("ZIP file as parent rejected", [&]{ inspectArchive(root + "/collision.zip", root + "/collision", {}, cancel); });
        makeZip(root + "/symlink.zip", {{"outside", "../../escape", 0120777}});
        rejects("Unix ZIP symlink rejected", [&]{ inspectArchive(root + "/symlink.zip", root + "/symlink", {}, cancel); });
        makeZip(root + "/fifo.zip", {{"fifo", "", 0010644}});
        rejects("Unix ZIP special file rejected", [&]{ inspectArchive(root + "/fifo.zip", root + "/fifo", {}, cancel); });
        makeZip(root + "/extra.zip", {{game.executable, entries[0].bytes, 0100755}, {"unexpected", "x"}});
        rejects("unsigned ZIP file rejected", [&]{ inspectArchive(root + "/extra.zip", root + "/extra", game.files, cancel); });
        check(!QFileInfo::exists(root + "/escape"), "no archive traversal escape written");
        atomicWrite(root + "/external-saves/save.json", "preserve me");
        store.uninstall(game.id);
        check(store.currentDirectory(game.id).isEmpty() && !QFileInfo::exists(root + "/games/fixture/versions"), "Mac uninstall removes managed versions");
        check(readFile(root + "/external-saves/save.json") == "preserve me", "Mac uninstall preserves external saves");
        store.install(game, zip, cancel); check(store.verify(game.id), "Mac reinstall after uninstall works");
        QTextStream(stdout) << "RESULT " << passed << " passed; " << failed << " failed\n";
        return failed ? 1 : 0;
    } catch(const std::exception &error) {
        QTextStream(stderr) << "FATAL " << error.what() << '\n';
        return 2;
    }
}
