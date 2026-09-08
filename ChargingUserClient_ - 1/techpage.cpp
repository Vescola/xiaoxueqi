#include "techpage.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QFont>
#include <QtGlobal>

TechPage::TechPage(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
}

void TechPage::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    static const QPixmap background(QStringLiteral(":/resources/images/auth_background.png"));
    if (background.isNull()) {
        painter.fillRect(rect(), QColor(QStringLiteral("#101b28")));
        return;
    }

    const QSize scaledSize = background.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
    const QPixmap scaled = background.scaled(scaledSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QPoint topLeft((width() - scaled.width()) / 2, (height() - scaled.height()) / 2);
    painter.drawPixmap(topLeft, scaled);

    // 深色主题：在浅蓝底图上叠加深色半透明蒙层，压暗为深蓝夜空效果
    QColor overlay(QStringLiteral("#0a131f"));
    overlay.setAlpha(186);
    painter.fillRect(rect(), overlay);

    const QString title = QStringLiteral("东软电动汽车充电桩应用管理平台");
    QFont titleFont = painter.font();
    titleFont.setPointSize(32);
    titleFont.setBold(true);
    painter.setFont(titleFont);

    const QRect titleRect(0, qMax(42, height() / 8), width(), 72);
    painter.setPen(QColor(0, 0, 0, 150));
    painter.drawText(titleRect.adjusted(2, 3, 2, 3), Qt::AlignCenter, title);
    painter.setPen(QColor(QStringLiteral("#e8f2ff")));
    painter.drawText(titleRect, Qt::AlignCenter, title);
}
