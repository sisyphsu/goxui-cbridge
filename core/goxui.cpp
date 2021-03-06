﻿//
// Created by sulin on 2017/9/23.
//

#include <QtGlobal>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickView>
#include <QQmlContext>
#include <QNetworkProxy>
#include <QFileDialog>
#include "singleapplication.h"
#include "goxui_p.h"

static PropertyNode *root = nullptr;
static QApplication *app = nullptr;
static QQmlApplicationEngine *engine = nullptr;
static QMap<QString, QObject*> contextProperties;
static void (*logCallback)(int type, char* file, int line, char* msg);

// log handler
void logHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QByteArray localMsg = msg.toLocal8Bit();
    if (logCallback != nullptr) {
        char *file = const_cast<char*>(context.file);
        int line = context.line;
        logCallback(type, file, line, const_cast<char*>(msg.toLocal8Bit().data()));
        return;
    }
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtInfoMsg:
        fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    }
}

// init QT context
API void ui_init(int argc, char **argv) {
    qInstallMessageHandler(logHandler);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QLoggingCategory::defaultCategory()->setEnabled(QtMsgType::QtDebugMsg, true);

    // For Qt5.9
    if (QT_VERSION_MINOR != 12) {
        qputenv("QSG_RENDER_LOOP", "basic");
        QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Software);
    }

    static QString NULL_Str;
    static int argNum = argc;
    if (isEnableSingleApplication()) {
        app = new SingleApplication(argNum, argv);
        if(static_cast<SingleApplication *>(app)->isSecondary() ) {
            qDebug() << "app repeated";
            app->exit(0);
        }
    } else {
        app = new QApplication(argNum, argv);
    }

    // init ui
    root = new PropertyNode(NULL_Str, nullptr);
    qmlRegisterType<WindowItem>("Goxui", 1, 0, "Window");
    qmlRegisterType<WindowTitleItem>("Goxui", 1, 0, "TitleBar");
    qmlRegisterType<EventItem>("Goxui", 1, 0, "Event");
    qmlRegisterType<HotKeyItem>("Goxui", 1, 0, "HotKey");

    engine = new QQmlApplicationEngine();
}

// setup logger
API void ui_set_logger(void (*logger)(int type, char* file, int line, char* msg)){
    logCallback = logger;
}

// Add an QObject into QML's context
API void ui_add_object(char *name, void *ptr) {
    QString nameStr(name);
    QObject *obj = static_cast<QObject *>(ptr);
    contextProperties.insert(nameStr, obj);
}

//  Add specified c field into QML
API int ui_add_field(char *name, int type, char *(*reader)(char *), void (*writer)(char *, char *)) {
    QString nameStr(name);
    Reader r = [=](void *ret) {
        qDebug() << "invoke c getter of property" << name;
        char *data = reader(name);
        qDebug() << "invoke c getter of property" << name << "done, result is:" << data;
        convertStrToPtr(data, type, ret);
        qDebug() << "convert to ptr success";
        // free(data); // memory leak???
    };
    Writer w = [=](void *arg) {
        QByteArray tmp = convertPtrToStr(arg, type);
        writer(name, tmp.toBase64().data());
    };
    switch (type) {
        case UI_TYPE_BOOL:
            return root->addField(nameStr, QVariant::Bool, r, w);
        case UI_TYPE_INT:
            return root->addField(nameStr, QVariant::Int, r, w);
        case UI_TYPE_LONG:
            return root->addField(nameStr, QVariant::LongLong, r, w);
        case UI_TYPE_DOUBLE:
            return root->addField(nameStr, QVariant::Double, r, w);
        case UI_TYPE_OBJECT:
            return root->addField(nameStr, QMetaType::QVariant, r, w);
        default:
            return root->addField(nameStr, QVariant::String, r, w);
    }
}

// Add specified c method into QML
API int ui_add_method(char *name, int retType, int argNum, char *(*callback)(char *, char *)) {
    QString nameStr(name);
    Callback call = [=](QVariant &ret, QVariantList &args) {
        auto param = QJsonDocument::fromVariant(args).toJson(QJsonDocument::Compact);
        qDebug() << "invoke method" << name << "with param: " << param;
        auto str = callback(name, param.toBase64().data());
        qDebug() << "invoke method" << name << "finish with result: "<< str;
        convertStrToVar(str, retType, ret);
        // free(str); // memory leak???
    };
    return root->addMethod(nameStr, argNum, call);
}

// Notify QML that specified data has changed
API int ui_notify_field(char *name) {
    QString nameStr(name);
    QVariant var;
    qDebug() << "field notify: " << name;
    return root->notifyProperty(nameStr, var);
}

// Trige specified QML event by name
API void ui_trigger_event(char *name, int dataType, char *data) {
    QString str(name);
    QVariant var;
    convertStrToVar(data, dataType, var);
    qDebug() << "ui_trigger_event: " << str <<var;
    for (auto item : EventItem::ReceverMap.values(str)) {
        if (item == nullptr) {
            continue;
        }
        item->notify(var);
    }
}

// Add RCC data to QResource as specified prefix
API void ui_add_resource(char *prefix, char *data) {
    QString rccPrefix(prefix);
    auto rccData = reinterpret_cast<uchar *>(data);
    QResource::registerResource(rccData, rccPrefix);
}

// Add file system path to QDir's resource search path
API void ui_add_resource_path(char *path) {
    QString resPath(path);
    QDir::addResourceSearchPath(resPath);
}

// Add file system path to QML import
API void ui_add_import_path(char *path) {
    QString importPath(path);
    engine->addImportPath(importPath);
}

// Map file system resource to specify QML prefix
API void ui_map_resource(char *prefix, char *path) {
    QString resPrefix(prefix);
    QString resPath(path);
    QDir::addSearchPath(resPrefix, resPath);
}

// start Application
API int ui_start(char *qml) {
    if (isEnableSingleApplication()) {
        QObject::connect(static_cast<SingleApplication *>(app), &SingleApplication::instanceStarted, [=]() {
            ui_trigger_event(const_cast<char*>("app_active"), UI_TYPE_VOID, nullptr);
        });
    }
    QObject::connect(app, &QApplication::applicationStateChanged, [=](Qt::ApplicationState state){
        if (state == Qt::ApplicationActive) {
            ui_trigger_event(const_cast<char*>("app_active"), UI_TYPE_VOID, nullptr);
        }
    });

    // setup root object
    root->buildMetaData();
    engine->rootContext()->setContextObject(root);

    // setup context properties
    contextProperties.insert("system", new UISystem(engine));
    for(auto name : contextProperties.keys()) {
        engine->rootContext()->setContextProperty(name, contextProperties.value(name));
    }

    // start~
    QString rootQML(qml);
    engine->load(rootQML);

    int code = app->exec();

    // clear
    delete engine;
    delete root;

    return code;
}

// TOOL: setup the http proxy of Application
API void ui_tool_set_http_proxy(char *host, int port) {
    QNetworkProxy proxy;
    proxy.setType(QNetworkProxy::HttpProxy);
    proxy.setHostName(host);
    proxy.setPort(static_cast<quint16>(port));
    QNetworkProxy::setApplicationProxy(proxy);
}
