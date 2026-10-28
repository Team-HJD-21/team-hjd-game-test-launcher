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
#include <windows.h>

int main(int argc,char **argv) {
    QGuiApplication app(argc,argv); app.setOrganizationName("TeamHJD");app.setApplicationName("GameTestLauncher");app.setApplicationVersion(HJD_VERSION);
    app.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    app.setWindowIcon(QIcon(":/assets/brand/TeamHJD-Logo.png"));
    QCommandLineParser parser;parser.addHelpOption();parser.addVersionOption();
    parser.addOptions({QCommandLineOption("data-root","Data directory (isolated test profiles)","path"),QCommandLineOption("launcher-root","Managed installation root","path"),QCommandLineOption("health-file","Bootstrap health acknowledgment","path"),QCommandLineOption("test-allow-loopback","Enable loopback HTTP fixtures only"),QCommandLineOption("capture","Capture this app window for layout review","path"),QCommandLineOption("quit-after-ms","Exit after an isolated UI review","ms"),QCommandLineOption("review-ui","Open a real UI surface for layout verification","surface"),QCommandLineOption("window-size","Review size in logical pixels","size")});parser.process(app);
    QString data=parser.value("data-root");if(data.isEmpty())data=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TeamHJD/Launcher";
    QDir().mkpath(data); QLockFile lock(data+"/client.lock"); if(!lock.tryLock(0)){MessageBoxW(nullptr,L"런처가 이미 실행 중입니다.",L"TeamHJD Game Test Launcher",MB_OK|MB_ICONINFORMATION);return 2;}
    Controller controller(data,parser.value("launcher-root"),parser.isSet("test-allow-loopback"));
    QQuickStyle::setStyle("Basic");QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("launcher",&controller);
    QObject::connect(&controller,&Controller::quitForUpdate,&app,&QCoreApplication::quit);
    engine.load(QUrl("qrc:/qml/Main.qml"));if(engine.rootObjects().isEmpty())return 3;
    auto *window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    std::unique_ptr<WindowFrame> frame;if(window)frame=std::make_unique<WindowFrame>(window);
    if(window&&parser.isSet("window-size")){auto parts=parser.value("window-size").split('x');if(parts.size()==2)window->resize(parts[0].toInt(),parts[1].toInt());}
    if(window&&parser.isSet("review-ui")){auto mode=parser.value("review-ui");QTimer::singleShot(700,&app,[window,mode]{window->setProperty("reviewMode",mode);});}
    bool acknowledged=false;auto health=parser.value("health-file");
    if(window&&!health.isEmpty())QObject::connect(window,&QQuickWindow::frameSwapped,&app,[&]{if(acknowledged)return;acknowledged=true;try{hjd::writeJson(health,{{"pid",QCoreApplication::applicationPid()},{"version",HJD_VERSION}});}catch(const hjd::Error&){}},Qt::QueuedConnection);
    if(window&&parser.isSet("capture")){auto target=parser.value("capture");QTimer::singleShot(2500,&app,[window,target]{QDir().mkpath(QFileInfo(target).absolutePath());window->grabWindow().save(target);});}
    bool ok=false;int duration=parser.value("quit-after-ms").toInt(&ok);if(ok&&duration>0)QTimer::singleShot(duration,&app,&QCoreApplication::quit);
    return app.exec();
}
