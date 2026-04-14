#pragma once
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QPropertyAnimation>

// Snackbar: kurze Meldung am unteren Rand, verschwindet nach 2,5s automatisch.
// Verwendung: Snackbar::show(parentWidget, "Fahrt gespeichert");

class Snackbar : public QWidget
{
    Q_OBJECT
public:
    static void show(QWidget *parent, const QString &message, int durationMs = 2500);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    explicit Snackbar(QWidget *parent, const QString &message, int durationMs);
    void positionSelf();

    QLabel             *m_label;
#if !defined(Q_OS_ANDROID)
    QPropertyAnimation *m_fadeAnim = nullptr;
#endif
};
