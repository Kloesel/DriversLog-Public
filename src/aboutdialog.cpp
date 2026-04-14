#include "aboutdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#if defined(Q_OS_ANDROID)
#  include <QQuickWidget>
#  include <QQuickItem>
#  include <QQmlContext>
#endif

// ── Android: MD3-Dialog per QML ──────────────────────────────────────────────
AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent
#if defined(Q_OS_ANDROID)
              , Qt::Window
#endif
             )
{
    setWindowTitle(tr("Ueber Fahrtenbuch"));

#if defined(Q_OS_ANDROID)
    // Qt 6.8 / Android 16: QQuickWidget statt QQuickView (geteilter EGL-Context).
    auto *pw = qobject_cast<QWidget*>(parent);
    auto *qw = new QQuickWidget(pw);
    qw->setAttribute(Qt::WA_DeleteOnClose);
    qw->setResizeMode(QQuickWidget::SizeRootObjectToView);
    qw->rootContext()->setContextProperty(
        QStringLiteral("appVersion"),        QStringLiteral(APP_VERSION));
    qw->rootContext()->setContextProperty(
        QStringLiteral("qtVersion"),         QStringLiteral(QT_VERSION_STR));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutAppName"),      tr("Driver's Log"));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutPlatform"),     tr("Windows / Android"));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutDatabase"),     tr("SQLite 3 (WAL)"));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutDistance"),     tr("ORS fastest route"));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutDescription"),  tr("Simple mileage log for recording business trips "
                                               "with automatic distance calculation and Wi-Fi sync."));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutCopyright"),    tr("\u00a9 2026 Kl\u00f6sel \u2013 All rights reserved"));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutLicenseLabel"), tr("License:"));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutLicenseText"),  tr("This program uses Qt %1, licensed under GNU LGPL v3. "
                                               "Qt is a registered trademark of The Qt Company Ltd.")
                                            .arg(QT_VERSION_STR));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutSourceLabel"),  tr("Qt source code:"));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutLabelQt"),      tr("Qt version"));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutLabelPlatform"),tr("Platform"));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutLabelDb"),      tr("Database"));
    qw->rootContext()->setContextProperty(
        QStringLiteral("aboutLabelDist"),    tr("Distance calculation"));

    QObject::connect(qw, &QQuickWidget::statusChanged, qw,
        [qw](QQuickWidget::Status s) {
            if (s == QQuickWidget::Error)
                for (const auto &e : qw->errors())
                    qWarning() << "AboutView QML:" << e.toString();
            if (s != QQuickWidget::Ready) return;
            if (auto *root = qw->rootObject())
                QObject::connect(root, SIGNAL(closeRequested()),
                                 qw, SLOT(deleteLater()));
        });

    qw->setSource(QUrl(QStringLiteral("qrc:/AboutView.qml")));
    if (pw) qw->setGeometry(0, 0, pw->width(), pw->height());
    qw->show();
    qw->raise();
    hide();

#else
    // ── Desktop ───────────────────────────────────────────────────────────────
    setMinimumWidth(420);
    auto *layout = new QVBoxLayout(this);

    auto *title = new QLabel("<h2>" + tr("Driver's Log") + "</h2>", this);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // Qt HTML-safe versions of translated strings
    const QString html = QString(
        "<p style='text-align:center;'>"
        "<b>%1</b> %2<br>"
        "<b>%3</b> %4<br>"
        "<b>%5</b> %6<br><br>"
        "%7<br><br>"
        "&copy; 2026 Kl&ouml;sel &ndash; %8<br><br>"
        "<small>%9 "
        "<a href='https://www.gnu.org/licenses/lgpl-3.0.html'>GNU LGPL v3</a>. "
        "%10<br>"
        "%11 <a href='https://download.qt.io'>download.qt.io</a></small>"
        "</p>")
        .arg(tr("Version:"),       APP_VERSION)
        .arg(tr("Qt version:"),    QT_VERSION_STR)
        .arg(tr("Platform:"),      tr("Windows / Android"))
        .arg(tr("Simple mileage log for recording business trips "
               "with automatic distance calculation and Wi-Fi sync."))
        .arg(tr("All rights reserved."))
        .arg(tr("This program uses Qt %1, licensed under GNU LGPL v3.").arg(QT_VERSION_STR))
        .arg(tr("Qt is a registered trademark of The Qt Company Ltd."))
        .arg(tr("Qt source code:"));

    auto *ver = new QLabel(html, this);
    ver->setAlignment(Qt::AlignCenter);
    ver->setTextFormat(Qt::RichText);
    ver->setOpenExternalLinks(true);
    layout->addWidget(ver);

    auto *closeBtn = new QPushButton(tr("OK"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);
#endif
}
