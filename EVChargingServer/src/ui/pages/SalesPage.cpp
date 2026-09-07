#include "ui/pages/SalesPage.h"
#include "ui_SalesPage.h"

#include "db/ServerDb.h"

SalesPage::SalesPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SalesPage)
{
    ui->setupUi(this);

    connect(ui->rangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SalesPage::onRangeChanged);
    connect(ui->refreshButton, &QPushButton::clicked,
            this, &SalesPage::refresh);

    ui->rangeCombo->setCurrentIndex(1);   // 默认近 30 日
    refresh();
}

SalesPage::~SalesPage()
{
    delete ui;
}

void SalesPage::refresh()
{
    const RevenueSummary s = ServerDb::instance().getRevenueSummary();
    ui->todayValue->setText(QString::number(s.today, 'f', 2));
    ui->monthValue->setText(QString::number(s.month, 'f', 2));
    ui->totalValue->setText(QString::number(s.total, 'f', 2));

    onRangeChanged();
}

void SalesPage::onRangeChanged()
{
    const int days = (ui->rangeCombo->currentIndex() == 0) ? 7 : 30;
    const auto trend = ServerDb::instance().getRevenueTrend(days);

    QList<QPair<QString, double>> data;
    for (const auto &kv : trend) {
        // 横轴只保留 "MM-dd"
        const QString label = kv.first.mid(5);   // "yyyy-MM-dd" -> "MM-dd"
        data.append(qMakePair(label, kv.second));
    }

    ui->trendChart->setTitle(QStringLiteral("近%1日营收趋势").arg(days));
    ui->trendChart->setUnit(QStringLiteral("元"));
    ui->trendChart->setData(data);
}
