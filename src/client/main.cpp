#include "Controller.h"
#include "WindowFrame.h"
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTimer>
#include <QTextStream>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc,char **argv) {
    QGuiApplication app(argc,argv); app.setOrganizationName("TeamHJD");app.setApplicationName("GameTestLauncher");app.setApplicationVersion(HJD_VERSION);
    app.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    app.setWindowIcon(QIcon(":/assets/brand/TeamHJD-Logo.png"));
    QCommandLineParser parser;parser.addHelpOption();parser.addVersionOption();
    parser.addOption(QCommandLineOption("invite","Connect using a team invitation file","path"));
    parser.addOption(QCommandLineOption("test-update-from","Isolated test only: compare launcher updates from this version","version"));
    parser.addOptions({QCommandLineOption("data-root","Data directory (isolated test profiles)","path"),QCommandLineOption("launcher-root","Managed installation root","path"),QCommandLineOption("health-file","Bootstrap health acknowledgment","path"),QCommandLineOption("test-allow-loopback","Enable loopback HTTP fixtures only"),QCommandLineOption("capture","Capture this app window for layout review","path"),QCommandLineOption("quit-after-ms","Exit after an isolated UI review","ms"),QCommandLineOption("review-ui","Open a real UI surface for layout verification","surface"),QCommandLineOption("window-size","Review size in logical pixels","size")});parser.process(app);
    const auto defaultData=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TeamHJD/Launcher";
    QString data=parser.value("data-root");if(data.isEmpty())data=defaultData;
    if(parser.isSet("test-update-from")){
        const auto launcherRoot=parser.value("launcher-root");
        const auto normal=[](const QString &path){return QDir::cleanPath(QFileInfo(path).absoluteFilePath());};
        if(!parser.isSet("test-allow-loopback")||!parser.isSet("data-root")||!parser.isSet("launcher-root")||
           !QDir::isAbsolutePath(data)||!QDir::isAbsolutePath(launcherRoot)||
           normal(data).compare(normal(defaultData),Qt::CaseInsensitive)==0||
           normal(launcherRoot).compare(normal(QCoreApplication::applicationDirPath()),Qt::CaseInsensitive)==0||
           normal(data).compare(normal(launcherRoot),Qt::CaseInsensitive)==0){
            QTextStream(stderr)<<"--test-update-from requires loopback testing and separate explicit absolute test data/launcher roots.\n";
            return 64;
        }
    }
    QDir().mkpath(data); QLockFile lock(data+"/client.lock"); if(!lock.tryLock(0)){
#ifdef Q_OS_WIN
        MessageBoxW(nullptr,L"런처가 이미 실행 중입니다.",L"TeamHJD Game Test Launcher",MB_OK|MB_ICONINFORMATION);
#else
        QTextStream(stderr)<<"TeamHJD launcher is already running.\n";
#endif
        return 2;
    }
    Controller controller(data,parser.value("launcher-root"),parser.isSet("test-allow-loopback"));
    if(parser.isSet("test-update-from")){
        try{controller.setTestUpdateFrom(parser.value("test-update-from"));}
        catch(const hjd::Error &error){QTextStream(stderr)<<error.what()<<'\n';return 64;}
    }
    QQuickStyle::setStyle("Basic");QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("launcher",&controller);
    QObject::connect(&controller,&Controller::quitForUpdate,&app,&QCoreApplication::quit);
    engine.load(QUrl("qrc:/qml/Main.qml"));if(engine.rootObjects().isEmpty())return 3;
    auto *window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    std::unique_ptr<WindowFrame> frame;if(window)frame=std::make_unique<WindowFrame>(window);
    if(parser.isSet("invite")){auto invite=QUrl::fromLocalFile(QFileInfo(parser.value("invite")).absoluteFilePath());QTimer::singleShot(600,&app,[&controller,invite]{controller.importInvite(invite);});}
    if(window&&parser.isSet("window-size")){auto parts=parser.value("window-size").split('x');if(parts.size()==2)window->resize(parts[0].toInt(),parts[1].toInt());}
    if(window&&parser.isSet("review-ui")){auto mode=parser.value("review-ui");QTimer::singleShot(700,&app,[window,mode]{window->setProperty("reviewMode",mode);});}
    bool acknowledged=false;auto health=parser.value("health-file");
    if(window)QObject::connect(window,&QQuickWindow::frameSwapped,&app,[&]{
        if(acknowledged)return;acknowledged=true;
        if(!health.isEmpty())try{hjd::writeJson(health,{{"pid",QCoreApplication::applicationPid()},{"version",HJD_VERSION}});}catch(const hjd::Error&){}
        QTimer::singleShot(0,&controller,&Controller::restoreCredentials);
    },Qt::QueuedConnection);
    if(window&&parser.isSet("capture")){auto target=parser.value("capture");QTimer::singleShot(2500,&app,[window,target]{QDir().mkpath(QFileInfo(target).absolutePath());window->grabWindow().save(target);});}
    bool ok=false;int duration=parser.value("quit-after-ms").toInt(&ok);if(ok&&duration>0)QTimer::singleShot(duration,&app,&QCoreApplication::quit);
    return app.exec();
}
