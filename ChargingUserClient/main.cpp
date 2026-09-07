#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QIODevice>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("ChargingUserClient"));
    application.setOrganizationName(QStringLiteral("NeusoftTraining"));
    application.setWindowIcon(QIcon(QStringLiteral(":/resources/icons/app.svg")));

    QFile styleFile(QStringLiteral(":/resources/styles/userclient.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        application.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    QFont font = application.font();
    font.setPointSize(10);
    application.setFont(font);

    MainWindow window;
    window.showMaximized();
    return application.exec();
}
