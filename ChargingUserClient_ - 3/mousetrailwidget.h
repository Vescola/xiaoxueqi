#ifndef MOUSETRAILWIDGET_H
#define MOUSETRAILWIDGET_H

#include <QElapsedTimer>
#include <QPointer>
#include <QVector>
#include <QWidget>

class QTimer;

class MouseTrailWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MouseTrailWidget(QWidget *host, QWidget *parent = nullptr);
    ~MouseTrailWidget() override;

    bool eventFilter(QObject *watched, QEvent *event) override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct TrailPoint
    {
        QPointF position;
        qint64 timestamp = 0;
    };

    void syncToHost();
    void recordMousePosition(const QPoint &globalPosition);
    void enableMouseTracking(QWidget *widget);
    void removeExpiredPoints();

    QPointer<QWidget> m_host;
    QVector<TrailPoint> m_points;
    QElapsedTimer m_clock;
    QTimer *m_fadeTimer = nullptr;
};

#endif // MOUSETRAILWIDGET_H
