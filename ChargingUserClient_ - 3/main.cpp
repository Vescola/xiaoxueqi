#include "mainwindow.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QIODevice>
#include <QPalette>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("ChargingUserClient"));
    application.setOrganizationName(QStringLiteral("NeusoftTraining"));
    application.setWindowIcon(QIcon(QStringLiteral(":/resources/icons/app.svg")));

    // 深色主题：设置全局调色板，保证未走 QSS 的控件（表格文字、下拉列表、
    // 富文本默认颜色、工具提示、QMessageBox 等）也能以深色正确显示。
    QPalette palette = application.palette();
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#0c1310")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#dce8e2")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#0e1713")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#131e19")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#dce8e2")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#141f1a")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#dce8e2")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#15553f")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#e8f7ee")));
    palette.setColor(QPalette::Link, QColor(QStringLiteral("#5ab0ff")));
    palette.setColor(QPalette::LinkVisited, QColor(QStringLiteral("#a78bfa")));
    palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#6b7d74")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#141f1a")));
    palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#dce8e2")));
    const QColor disabledText(QStringLiteral("#5c6f65"));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    application.setPalette(palette);

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
