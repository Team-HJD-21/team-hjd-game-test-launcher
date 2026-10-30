#include "CloudflareLogin.h"
#include "Core.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QUuid>
#ifdef Q_OS_WIN
#include <windows.h>
#include <sddl.h>
#elif defined(Q_OS_MACOS)
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#endif

#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#endif

namespace hjd {
namespace {
constexpr qint64 maximumOutput = 128 * 1024;
constexpr qint64 authenticationTimeoutMs = 5 * 60 * 1000;

void erase(QByteArray &bytes) {
    if(!bytes.isEmpty()) {
#ifdef Q_OS_WIN
        SecureZeroMemory(bytes.data(),size_t(bytes.size()));
#elif defined(Q_OS_MACOS)
        explicit_bzero(bytes.data(),size_t(bytes.size()));
#endif
    }
    bytes.clear();
}

#ifdef Q_OS_WIN
QString currentSid() {
    HANDLE handle{};
    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &handle))
        fail(QStringLiteral("Windows 인증 임시 폴더를 준비할 수 없습니다."));
    DWORD length = 0;
    GetTokenInformation(handle, TokenUser, nullptr, 0, &length);
    QByteArray buffer(length, 0);
    if(!length || !GetTokenInformation(handle, TokenUser, buffer.data(), length, &length)) {
        CloseHandle(handle);
        fail(QStringLiteral("Windows 사용자 보호 정보를 읽을 수 없습니다."));
    }
    LPWSTR sid{};
    bool ok = ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid);
    CloseHandle(handle);
    if(!ok) fail(QStringLiteral("Windows 사용자 보호 정보를 확인할 수 없습니다."));
    QString result = QString::fromWCharArray(sid);
    LocalFree(sid);
    return result;
}

bool insideScratch(const QString &path, const QString &root) {
    const auto clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    return clean.compare(root, Qt::CaseInsensitive) == 0 ||
           clean.startsWith(root + '/', Qt::CaseInsensitive);
}

void checkScratchJunction(const QFileInfo &entry, const QString &root) {
    // Windows ShellExecute/WinINet may create Content.IE5 -> IE in the temporary
    // USERPROFILE. Accept directory junctions only when both their declared and
    // fully resolved targets stay inside this object's exact UUID directory.
    const auto target = entry.junctionTarget();
    const auto resolved = QFileInfo(target).canonicalFilePath();
    if(!entry.isJunction() || target.isEmpty() || resolved.isEmpty() ||
       !insideScratch(target, root) || !insideScratch(resolved, root))
        fail(QStringLiteral("인증 임시 폴더 밖으로 연결된 경로는 정리할 수 없습니다."));
}

void findScratchJunctions(const QString &directory, const QString &root, QStringList &junctions) {
    noLinks(directory);
    const auto entries = QDir(directory).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                                                       QDir::NoSort);
    for(const auto &entry : entries) {
        const auto native = QDir::toNativeSeparators(entry.absoluteFilePath());
        const auto attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()));
        if(attributes == INVALID_FILE_ATTRIBUTES)
            fail(QStringLiteral("인증 임시 폴더의 항목을 확인하지 못했습니다."));
        if(attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            checkScratchJunction(entry, root);
            junctions.append(entry.absoluteFilePath()); // Never recurse through it.
        } else if(attributes & FILE_ATTRIBUTE_DIRECTORY) {
            findScratchJunctions(entry.absoluteFilePath(), root, junctions);
        }
    }
}

void removePlainScratchTree(const QString &directory) {
    noLinks(directory);
    const auto entries = QDir(directory).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                                                       QDir::NoSort);
    for(const auto &entry : entries) {
        noLinks(entry.absoluteFilePath());
        if(entry.isDir()) removePlainScratchTree(entry.absoluteFilePath());
        else if(!QFile::remove(entry.absoluteFilePath()))
            fail(QStringLiteral("임시 인증 파일을 정리하지 못했습니다."));
    }
    const auto native = QDir::toNativeSeparators(directory);
    if(!RemoveDirectoryW(reinterpret_cast<LPCWSTR>(native.utf16())))
        fail(QStringLiteral("임시 인증 폴더를 정리하지 못했습니다."));
}

class PrivateScratch {
public:
    explicit PrivateScratch(const QString &parent) {
        const auto absolute = QDir::cleanPath(QFileInfo(parent).absoluteFilePath());
        if(parent.isEmpty() || !QDir::isAbsolutePath(parent))
            fail(QStringLiteral("인증 임시 폴더의 전체 경로가 필요합니다."));
        noLinks(absolute);
        if(!QDir().mkpath(absolute)) fail(QStringLiteral("인증 임시 폴더를 만들지 못했습니다."));
        m_path = absolute + "/login-" + QUuid::createUuid().toString(QUuid::Id128);
        m_parent = absolute;
        const auto sddl = QStringLiteral("D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;%1)").arg(currentSid());
        PSECURITY_DESCRIPTOR descriptor{};
        if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(reinterpret_cast<LPCWSTR>(sddl.utf16()), SDDL_REVISION_1, &descriptor, nullptr))
            fail(QStringLiteral("인증 임시 폴더 보호를 설정할 수 없습니다."));
        SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
        const auto native = QDir::toNativeSeparators(m_path);
        bool created = CreateDirectoryW(reinterpret_cast<LPCWSTR>(native.utf16()), &attributes);
        LocalFree(descriptor);
        if(!created) fail(QStringLiteral("보호된 인증 임시 폴더를 만들지 못했습니다."));
        m_owned = true;
    }

    ~PrivateScratch() { try { remove(); } catch(...) {} }
    QString path() const { return m_path; }
    void remove() {
        if(!m_owned) return;
        // Only the exact UUID directory made by this object may be removed.
        if(QFileInfo(m_path).dir().absolutePath() != m_parent ||
           !QFileInfo(m_path).fileName().startsWith("login-"))
            fail(QStringLiteral("인증 임시 폴더의 경로를 확인하지 못했습니다."));
        noLinks(m_path);
        if(QFileInfo::exists(m_path)) {
            QStringList junctions;
            findScratchJunctions(m_path, m_path, junctions);
            // Preflight all targets before any deletion. RemoveDirectoryW on a
            // junction deletes the link itself, never its target or contents.
            for(const auto &junction : junctions) {
                noLinks(QFileInfo(junction).absolutePath());
                const auto native = QDir::toNativeSeparators(junction);
                if(!RemoveDirectoryW(reinterpret_cast<LPCWSTR>(native.utf16())))
                    fail(QStringLiteral("임시 인증 연결 폴더를 정리하지 못했습니다."));
            }
            // Recheck every entry during ordinary deletion. A newly introduced
            // reparse point is rejected instead of traversed.
            removePlainScratchTree(m_path);
        }
        m_owned = false;
    }
private:
    QString m_path, m_parent;
    bool m_owned = false;
};
#elif defined(Q_OS_MACOS)
void removePlainScratchTree(const QString &directory) {
    noLinks(directory);
    const auto entries=QDir(directory).entryInfoList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System,QDir::NoSort);
    for(const auto &entry:entries) {
        noLinks(entry.absoluteFilePath());
        if(entry.isDir())removePlainScratchTree(entry.absoluteFilePath());
        else if(!QFile::remove(entry.absoluteFilePath()))fail(QStringLiteral("임시 인증 파일을 정리하지 못했습니다."));
    }
    if(!QDir().rmdir(directory))fail(QStringLiteral("임시 인증 폴더를 정리하지 못했습니다."));
}
class PrivateScratch {
public:
    explicit PrivateScratch(const QString &parent) {
        if(parent.isEmpty()||!QDir::isAbsolutePath(parent))fail(QStringLiteral("인증 임시 폴더의 전체 경로가 필요합니다."));
        m_parent=QDir::cleanPath(QFileInfo(parent).absoluteFilePath());
        noLinks(m_parent);
        if(!QDir().mkpath(m_parent))fail(QStringLiteral("인증 임시 폴더를 만들지 못했습니다."));
        m_path=m_parent+"/login-"+QUuid::createUuid().toString(QUuid::Id128);
        if(::mkdir(QFile::encodeName(m_path).constData(),0700)!=0)fail(QStringLiteral("보호된 인증 임시 폴더를 만들지 못했습니다."));
        struct stat info{};
        if(lstat(QFile::encodeName(m_path).constData(),&info)!=0||!S_ISDIR(info.st_mode)||info.st_uid!=getuid()||(info.st_mode&077)!=0)
            fail(QStringLiteral("인증 임시 폴더 보호를 확인하지 못했습니다."));
        m_device=info.st_dev;m_inode=info.st_ino;m_owned=true;
    }
    ~PrivateScratch(){try{remove();}catch(...) {}}
    QString path() const{return m_path;}
    void remove() {
        if(!m_owned)return;
        if(QFileInfo(m_path).dir().absolutePath()!=m_parent||!QFileInfo(m_path).fileName().startsWith("login-"))fail(QStringLiteral("인증 임시 폴더의 경로를 확인하지 못했습니다."));
        noLinks(m_path);
        struct stat info{};
        if(lstat(QFile::encodeName(m_path).constData(),&info)!=0||!S_ISDIR(info.st_mode)||info.st_uid!=getuid()||info.st_dev!=m_device||info.st_ino!=m_inode)
            fail(QStringLiteral("인증 임시 폴더의 소유 정보를 확인하지 못했습니다."));
        removePlainScratchTree(m_path);m_owned=false;
    }
private:
    QString m_path,m_parent;
    dev_t m_device{};ino_t m_inode{};
    bool m_owned=false;
};
#endif

QJsonObject decodeObject(const QByteArray &value) {
    auto decoded = QByteArray::fromBase64Encoding(value, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if(!decoded) fail(QStringLiteral("인증 응답 형식이 올바르지 않습니다."));
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(decoded.decoded, &error);
    erase(decoded.decoded);
    if(error.error != QJsonParseError::NoError || !document.isObject())
        fail(QStringLiteral("인증 응답 형식이 올바르지 않습니다."));
    return document.object();
}

void stopChild(QProcess &process) {
    if(process.state() == QProcess::NotRunning) return;
    process.terminate();
    if(!process.waitForFinished(750)) { process.kill(); process.waitForFinished(2000); }
}
}

CloudflareSession CloudflareLogin::inspect(const QByteArray &jwt, const QString &expectedIssuer,
                                           const QString &expectedAudience) {
    if(jwt.isEmpty() || jwt.size() > 64 * 1024 || expectedIssuer.isEmpty() || expectedAudience.isEmpty())
        fail(QStringLiteral("이메일 인증 설정 또는 응답이 올바르지 않습니다."));
    static const QRegularExpression encoded(QStringLiteral("^[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+$"));
    if(!encoded.match(QString::fromLatin1(jwt)).hasMatch())
        fail(QStringLiteral("인증 응답을 읽을 수 없습니다."));
    auto parts = jwt.split('.');
    const auto header = decodeObject(parts[0]);
    const auto claims = decodeObject(parts[1]);
    if(header["alg"].toString() != "RS256" || header["kid"].toString().isEmpty() ||
       claims["iss"].toString() != expectedIssuer || claims["type"].toString() != "app")
        fail(QStringLiteral("요청한 테스트 서버의 인증 응답이 아닙니다."));
    bool audienceMatches = claims["aud"].toString() == expectedAudience;
    for(const auto &value : claims["aud"].toArray())
        if(value.toString() == expectedAudience) audienceMatches = true;
    if(!audienceMatches) fail(QStringLiteral("다른 서비스의 인증 응답입니다."));
    const auto now = QDateTime::currentSecsSinceEpoch();
    const auto expires = claims["exp"].toInteger(0);
    const auto notBefore = claims["nbf"].toInteger(0);
    if(expires <= now + 30 || notBefore > now + 120 || claims["iat"].toInteger(now) > now + 120)
        fail(QStringLiteral("이메일 인증의 유효기간이 지났습니다. 다시 인증해 주세요."));
    return {jwt, QDateTime::fromSecsSinceEpoch(expires, Qt::UTC)};
}

CloudflareSession CloudflareLogin::authenticate(const QUrl &application, const QString &helperExecutable,
                                               const QString &privateCacheRoot, const QString &expectedIssuer,
                                               const QString &expectedAudience, std::atomic_bool &cancel,
                                               const std::function<void(const QString &)> &status) {
    if(cancel) fail(QStringLiteral("취소했습니다."));
    if(!allowedServer(application) || application.port(443) != 443 ||
       !QStringList{"", "/"}.contains(application.path()) ||
       expectedIssuer.isEmpty() || expectedAudience.isEmpty())
        fail(QStringLiteral("이메일 인증 서버 설정이 올바르지 않습니다."));
    QFileInfo helper(helperExecutable);
    if(!helper.isAbsolute() || !helper.isFile() || helper.fileName().compare(QString::fromLatin1(helperName), Qt::CaseInsensitive) != 0)
        fail(QStringLiteral("이메일 인증 구성 요소가 없습니다. 최신 런처로 복구해 주세요."));
    noLinks(helper.absoluteFilePath());
    if(fileHash(helper.absoluteFilePath(), &cancel) != QString::fromLatin1(helperSha256))
        fail(QStringLiteral("이메일 인증 구성 요소를 확인하지 못했습니다. 최신 런처로 복구해 주세요."));

    PrivateScratch scratch(privateCacheRoot);
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    // go-homedir prefers HOME. Keep the real Windows USERPROFILE so the helper's
    // cmd/start browser launch cannot give the browser a temporary user profile.
    // This HOME override exists only in the helper child, never globally.
    environment.insert("HOME", QDir::toNativeSeparators(scratch.path()));
    // Ambient cloudflared settings must not enable file/debug logging or select
    // unrelated credentials for this one-time authentication child.
    for(const auto &name : environment.keys())
        if(name.startsWith("TUNNEL_", Qt::CaseInsensitive) || name.startsWith("CLOUDFLARED_", Qt::CaseInsensitive))
            environment.remove(name);
    environment.insert("CFDPATH", QDir::toNativeSeparators(scratch.path()));
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.setProgram(helper.absoluteFilePath());
    process.setWorkingDirectory(helper.absolutePath());
    process.setArguments({"access", "login", "--app=" + application.toString(QUrl::FullyEncoded), "--no-verbose", "--auto-close"});
    process.start();
    if(!process.waitForStarted(10000)) fail(QStringLiteral("이메일 인증 창을 열지 못했습니다."));
    if(status) status(QStringLiteral("브라우저에서 이메일 인증을 마쳐 주세요."));

    QByteArray output;
    QElapsedTimer elapsed;
    elapsed.start();
    try {
        while(process.state() != QProcess::NotRunning) {
            process.waitForFinished(100);
            auto next = process.readAllStandardOutput();
            auto diagnostic = process.readAllStandardError();
            erase(diagnostic); // May contain the private transfer URL: never log it.
            if(next.size() > maximumOutput - output.size()) {
                erase(next);
                fail(QStringLiteral("이메일 인증 응답이 너무 큽니다."));
            }
            output += next;
            erase(next);
            if(cancel) fail(QStringLiteral("취소했습니다."));
            if(elapsed.elapsed() >= authenticationTimeoutMs)
                fail(QStringLiteral("이메일 인증 시간이 초과되었습니다. 다시 시도해 주세요."));
        }
        auto tail = process.readAllStandardOutput();
        if(tail.size() > maximumOutput - output.size()) { erase(tail); fail(QStringLiteral("이메일 인증 응답이 너무 큽니다.")); }
        output += tail; erase(tail);
        auto diagnostic = process.readAllStandardError(); erase(diagnostic);
        if(process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
            fail(QStringLiteral("이메일 인증을 완료하지 못했습니다. 브라우저에서 다시 시도해 주세요."));
        auto trimmed = output.trimmed();
        auto result = inspect(trimmed, expectedIssuer, expectedAudience);
        erase(trimmed);
        erase(output);
        scratch.remove();
        return result;
    } catch(...) {
        stopChild(process);
        erase(output);
        auto remaining = process.readAllStandardOutput(); erase(remaining);
        remaining = process.readAllStandardError(); erase(remaining);
        throw;
    }
}
}
