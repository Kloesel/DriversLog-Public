// snackbar.cpp
// MD3 Snackbar – kurze Benachrichtigung am unteren Rand
//
// Android-Fix: QPropertyAnimation auf "windowOpacity" wird vom Qt-Android-
// Plugin nicht unterstützt → "This plugin does not support setting window
// opacity" erscheint hunderte Male im LogCat und verstopft die Rendering-Queue.
// Auf Android: kein Fade, sofortiges Show/Hide via QTimer.

#include "snackbar.h"
#include <QVBoxLayout>
#include <QScreen>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>

Snackbar::Snackbar(QWidget *parent, const QString &message, int durationMs)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setObjectName("md3Snackbar");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);

    m_label = new QLabel(message, this);
    m_label->setStyleSheet(
        "QLabel {"
        "  color: #FFFFFF;"
        "  font-size: 14px;"
        "  background: transparent;"
        "}");
    m_label->setWordWrap(true);
    layout->addWidget(m_label);

    positionSelf();

#if defined(Q_OS_ANDROID)
    // Android: setWindowOpacity() wird nicht unterstützt → kein Fade,
    // einfach sofort vollständig anzeigen und nach durationMs löschen.
    QWidget::show();
    QTimer::singleShot(durationMs, this, &QObject::deleteLater);
#else
    // Desktop: sanftes Ein-/Ausblenden mit QPropertyAnimation
    m_fadeAnim = new QPropertyAnimation(this, "windowOpacity");
    m_fadeAnim->setDuration(200);
    m_fadeAnim->setStartValue(0.0);
    m_fadeAnim->setEndValue(1.0);

    setWindowOpacity(0.0);
    QWidget::show();
    m_fadeAnim->start();

    QTimer::singleShot(durationMs, this, [this]() {
        auto *fadeOut = new QPropertyAnimation(this, "windowOpacity");
        fadeOut->setDuration(300);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);
        connect(fadeOut, &QPropertyAnimation::finished, this, &QObject::deleteLater);
        fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
    });
#endif
}

void Snackbar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
    p.fillPath(path, QColor(0x31, 0x36, 0x38));
}

void Snackbar::positionSelf()
{
    if (!parentWidget()) return;
    QRect pr = parentWidget()->rect();
    int w = qMin(pr.width() - 32, 480);
    int h = sizeHint().height() + 24;
    int x = pr.left() + (pr.width() - w) / 2;
    int y = pr.bottom() - h - 16;
    setGeometry(parentWidget()->mapToGlobal(QPoint(x, y)).x(),
                parentWidget()->mapToGlobal(QPoint(x, y)).y(),
                w, h);
}

void Snackbar::show(QWidget *parent, const QString &message, int durationMs)
{
    new Snackbar(parent, message, durationMs);
}
