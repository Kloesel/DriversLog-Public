#include "swipefilter.h"
#include <QWidget>
#include <QTouchEvent>
#include <QMouseEvent>
#include <cmath>

static constexpr int   kMinSwipeDistance = 60;
static constexpr int   kMaxVerticalDrift = 80;
static constexpr qreal kMinVelocity      = 0.3;

SwipeFilter::SwipeFilter(QObject *watched, QObject *parent)
    : QObject(parent)
{
    if (watched)
        installOnParent(watched);
}

void SwipeFilter::installOnParent(QObject *watched)
{
    if (!watched) return;
    watched->installEventFilter(this);
    if (auto *w = qobject_cast<QWidget *>(watched))
        w->setAttribute(Qt::WA_AcceptTouchEvents);
}

bool SwipeFilter::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)

    switch (event->type()) {

    /* ── Touch (Qt6) ─────────────────────────────────── */
    case QEvent::TouchBegin: {
        auto *te = static_cast<QTouchEvent *>(event);
        if (te->points().isEmpty()) break;
        m_startPos    = te->points().first().pressPosition().toPoint();
        m_lastPos     = m_startPos;
        m_startTime   = QDateTime::currentMSecsSinceEpoch();
        m_active      = true;
        m_gestureDone = false;
        return false;
    }
    case QEvent::TouchUpdate: {
        if (!m_active) break;
        auto *te = static_cast<QTouchEvent *>(event);
        if (te->points().isEmpty()) break;
        m_lastPos = te->points().first().position().toPoint();
        int dx = m_lastPos.x() - m_startPos.x();
        int dy = m_lastPos.y() - m_startPos.y();
        if (std::abs(dy) > std::abs(dx) && std::abs(dy) > 10)
            return false;
        return false;
    }
    case QEvent::TouchEnd: {
        if (!m_active || m_gestureDone) { m_active = false; break; }
        auto *te = static_cast<QTouchEvent *>(event);
        if (!te->points().isEmpty())
            m_lastPos = te->points().first().position().toPoint();
        m_active = false;
        evaluateSwipe();
        break;
    }

    /* ── Maus-Fallback ───────────────────────────────── */
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            m_startPos      = me->pos();
            m_lastPos       = m_startPos;
            m_startTime     = QDateTime::currentMSecsSinceEpoch();
            m_mouseTracking = true;
            m_gestureDone   = false;
        }
        break;
    }
    case QEvent::MouseMove:
        if (m_mouseTracking)
            m_lastPos = static_cast<QMouseEvent *>(event)->pos();
        break;
    case QEvent::MouseButtonRelease:
        if (m_mouseTracking && !m_gestureDone) {
            m_lastPos       = static_cast<QMouseEvent *>(event)->pos();
            m_mouseTracking = false;
            evaluateSwipe();
        }
        break;
    default: break;
    }
    return false;
}

void SwipeFilter::evaluateSwipe()
{
    int    dx = m_lastPos.x() - m_startPos.x();
    int    dy = m_lastPos.y() - m_startPos.y();
    qint64 dt = QDateTime::currentMSecsSinceEpoch() - m_startTime;

    if (std::abs(dx) < kMinSwipeDistance)       return;
    if (std::abs(dy) > kMaxVerticalDrift)        return;
    if (dt == 0)                                  return;
    if ((qreal)std::abs(dx) / dt < kMinVelocity) return;

    m_gestureDone = true;
    if (dx > 0) emit swipedRight();
    else        emit swipedLeft();
}
