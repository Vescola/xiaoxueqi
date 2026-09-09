#ifndef TECHPAGE_H
#define TECHPAGE_H

#include <QWidget>

class TechPage : public QWidget
{
    Q_OBJECT

public:
    explicit TechPage(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // TECHPAGE_H
