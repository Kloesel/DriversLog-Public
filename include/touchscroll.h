#pragma once
// touchscroll.h – Touch-Scrolling für Android.
// QScroller MUSS auf viewport() und ScrollPerPixel gesetzt werden.

#if defined(Q_OS_ANDROID)
#  include <QScroller>
#  include <QScrollerProperties>
#  include <QAbstractItemView>
#  include <QScrollArea>

inline void applyScrollerProps(QObject *target)
{
    QScrollerProperties p;
    // Kein Überschwingen über Anfang/Ende
    p.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 1.0);
    p.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.0);
    p.setScrollMetric(QScrollerProperties::OvershootScrollTime,           0.0);
    // Flüssige Verzögerung
    p.setScrollMetric(QScrollerProperties::DecelerationFactor,            0.15);
    QScroller::scroller(target)->setScrollerProperties(p);
}

inline void enableTouchScroll(QAbstractItemView *view)
{
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // ScrollPerPixel ist PFLICHT für QScroller – ohne es scrollt er nur zeilenweise
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    QScroller::grabGesture(view->viewport(), QScroller::TouchGesture);
    applyScrollerProps(view->viewport());
}

inline void enableTouchScroll(QScrollArea *sa)
{
    sa->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QScroller::grabGesture(sa->viewport(), QScroller::TouchGesture);
    applyScrollerProps(sa->viewport());
}

#else
#include <QAbstractItemView>
#include <QScrollArea>
inline void enableTouchScroll(QAbstractItemView *) {}
inline void enableTouchScroll(QScrollArea *)       {}
#endif
