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
        painter.fillRect(rect(), QColor(QStringLiteral("#eef6ff")));
        return;
    }

    const QSize scaledSize = background.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
    const QPixmap scaled = background.scaled(scaledSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QPoint topLeft((width() - scaled.width()) / 2, (height() - scaled.height()) / 2);
    painter.drawPixmap(topLeft, scaled);

    QColor wash(QStringLiteral("#ffffff"));
    wash.setAlpha(18);
    painter.fillRect(rect(), wash);

    const QString title = QStringLiteral("东软电动汽车充电桩应用管理平台");
    QFont titleFont = painter.font();
    titleFont.setPointSize(32);
    titleFont.setBold(true);
    painter.setFont(titleFont);

    const QRect titleRect(0, qMax(42, height() / 8), width(), 72);
    painter.setPen(QColor(255, 255, 255, 180));
    painter.drawText(titleRect.adjusted(2, 3, 2, 3), Qt::AlignCenter, title);
    painter.setPen(QColor(QStringLiteral("#0f3b66")));
    painter.drawText(titleRect, Qt::AlignCenter, title);
}
