#pragma once
#include <QObject>
#include <QEvent>
#include <QPoint>
#include <QDateTime>

// SwipeFilter: erkennt horizontale Wischgesten (Touch + Maus-Fallback)
// Verwendung:
//   m_swipeFilter = new SwipeFilter(watchedWidget, this);
//   m_swipeFilter->installOnParent(otherWidget);  // optional: weiteres Widget

class SwipeFilter : public QObject
{
    Q_OBJECT
public:
    // watched: Widget auf dem der Filter sofort installiert wird
    explicit SwipeFilter(QObject *watched = nullptr, QObject *parent = nullptr);

    // Zusätzliches Widget registrieren
    void installOnParent(QObject *watched);

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void swipedLeft();
    void swipedRight();

private:
    void evaluateSwipe();

    QPoint  m_startPos;
    QPoint  m_lastPos;
    qint64  m_startTime   = 0;
    bool    m_active      = false;
    bool    m_gestureDone = false;
    bool    m_mouseTracking = false;
};
