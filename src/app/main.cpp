#include "app/AppSettings.h"
#include "app/MainWindow.h"
#include "ui/UiContext.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QTimer>

int main(int argc, char* argv[])
{
    // DPI awareness: crisp rendering on 1080p, 1440p and 4K, including fractional Windows scaling.
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ClassBoard"));
    QApplication::setOrganizationName(QStringLiteral("ClassBoard"));
    QApplication::setApplicationVersion(QStringLiteral(CLASSBOARD_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/app/classboard.svg")));
    Q_INIT_RESOURCE(classboard);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("ClassBoard - digital classroom board"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("lesson"), QStringLiteral("Lesson (.classboard) to open."));
    QCommandLineOption screenshot(QStringLiteral("screenshot"),
                                  QStringLiteral("Render the window to an image after start-up and exit (diagnostics)."),
                                  QStringLiteral("file"));
    QCommandLineOption scale(QStringLiteral("ui-scale"), QStringLiteral("Override the interface size (e.g. 1.5)."),
                             QStringLiteral("factor"));
    parser.addOption(screenshot);
    parser.addOption(scale);
    parser.process(app);

    cb::UiContext ui;
    ui.theme.load(QStringLiteral(":/themes/dark.json"));
    cb::AppSettings settings;
    if (parser.isSet(scale))
        settings.setUiScale(parser.value(scale).toDouble());

    cb::MainWindow window(ui, settings);
    window.show();
    const QStringList args = parser.positionalArguments();
    window.startup(args.isEmpty() ? QString() : args.first());

    if (parser.isSet(screenshot)) {
        const QString path = parser.value(screenshot);
        QTimer::singleShot(1500, &window, [&window, path]() {
            window.grab().save(path);
            QCoreApplication::quit();
        });
    }
    return app.exec();
}
