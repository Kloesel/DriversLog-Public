#include "drawerwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QMouseEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include "touchscroll.h"
#include <QDebug>
#include <QTimer>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QScreen>

// Gibt 1.25 zurück wenn das Gerät ein Tablet ist (kleinste logische Seite >= 600 dp),
// sonst 1.0. Wird einmal pro App-Start berechnet und gecacht.
static qreal tabletScale()
{
    static qreal s_scale = -1.0;
    if (s_scale < 0.0) {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            QSizeF logicalSize = screen->size();          // in logischen Pixeln (Qt-intern skaliert)
            qreal smallest    = qMin(logicalSize.width(), logicalSize.height());
            s_scale = (smallest >= 600.0) ? 1.25 : 1.0;
            qDebug() << "[DrawerWidget] screen logical:" << logicalSize
                     << "→ tabletScale =" << s_scale;
        } else {
            s_scale = 1.0;
        }
    }
    return s_scale;
}

// UserRole map:
//   Qt::UserRole      → icon QString
//   Qt::UserRole + 1  → hidden flag (bool)
//   Qt::UserRole + 2  → isSeparator (bool)

/* ──────────────────────────────────────────────
   Md3NavDelegate
   ────────────────────────────────────────────── */
class Md3NavDelegate : public QStyledItemDelegate
{
public:
    explicit Md3NavDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &idx) const override
    {
        // Trennlinie
        if (idx.data(Qt::UserRole + 2).toBool()) {
            p->save();
            p->setPen(QPen(QColor(0xDC, 0xE4, 0xE9), 1));
            int y = opt.rect.top() + opt.rect.height() / 2;
            p->drawLine(opt.rect.left() + 16, y, opt.rect.right() - 16, y);
            p->restore();
            return;
        }

        p->save();
        p->setRenderHint(QPainter::Antialiasing);

        const bool  selected = opt.state & QStyle::State_Selected;
        const qreal dp       = tabletScale();

        if (selected) {
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(0xDC, 0xE4, 0xE9));
            p->drawRoundedRect(opt.rect.adjusted(8, 2, -8, -2), 28, 28);
        }

        QString icon = idx.data(Qt::UserRole).toString();
        QFont iconFont = p->font();
        iconFont.setPixelSize(qRound(28 * dp));

        // NotoColorEmoji ist auf allen Android-Geräten vorhanden.
        // Einmalig laden und Family-Namen cachen.
        static QString emojiFamilyCache;
        static bool emojiLoaded = false;
        if (!emojiLoaded) {
            emojiLoaded = true;
            QStringList candidates = {
                "/system/fonts/SamsungColorEmoji.ttf",
                "/system/fonts/NotoColorEmojiLegacy.ttf",
                "/system/fonts/NotoColorEmoji.ttf"
            };
            for (const QString &path : candidates) {
                int id = QFontDatabase::addApplicationFont(path);
                if (id != -1) {
                    QStringList families = QFontDatabase::applicationFontFamilies(id);
                    if (!families.isEmpty()) {
                        emojiFamilyCache = families.first();
                        qDebug() << "[Drawer] Emoji-Font geladen:" << emojiFamilyCache << "von" << path;
                        break;
                    }
                }
            }
        }
        if (!emojiFamilyCache.isEmpty())
            iconFont.setFamily(emojiFamilyCache);

        p->setFont(iconFont);
        p->setPen(selected ? QColor(0x00, 0x64, 0x93) : QColor(0x44, 0x47, 0x4E));
        const int iconAreaW = qRound(44 * dp);
        const int iconLeft  = qRound(12 * dp);
        QRect iconRect(opt.rect.left() + iconLeft, opt.rect.top(), iconAreaW, opt.rect.height());
        p->drawText(iconRect, Qt::AlignVCenter | Qt::AlignLeft, icon);

        QFont labelFont = p->font();
        labelFont.setPixelSize(qRound(14 * dp));
        labelFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
        p->setFont(labelFont);
        p->setPen(selected ? QColor(0x1A, 0x1C, 0x1E) : QColor(0x44, 0x47, 0x4E));
        const int textLeft = qRound(64 * dp);
        const int textMarg = qRound(72 * dp);
        QRect textRect(opt.rect.left() + textLeft, opt.rect.top(),
                       opt.rect.width() - textMarg, opt.rect.height());
        p->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                    idx.data(Qt::DisplayRole).toString());

        p->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &idx) const override
    {
        const qreal dp = tabletScale();
        if (idx.data(Qt::UserRole + 2).toBool())
            return QSize(0, qRound(16 * dp));   // Trennlinie: schmal
        return QSize(0, qRound(56 * dp));
    }
};

/* ──────────────────────────────────────────────
   DrawerWidget
   ────────────────────────────────────────────── */
DrawerWidget::DrawerWidget(QWidget *parent)
    : QWidget(parent)
    , m_overlay(new QWidget(parent))
    , m_panel(new QWidget(this))
    , m_listView(new QListView(m_panel))
    , m_model(new QStandardItemModel(this))
    , m_currentIndex(-1)
    , m_open(false)
{
    m_overlay->setObjectName("md3Overlay");
    m_overlay->setStyleSheet("background:rgba(0,0,0,0);");
    m_overlay->hide();
    m_overlay->installEventFilter(this);

    m_panel->setObjectName("md3Drawer");
    const int panelW = qRound(280 * tabletScale());
    m_panel->setFixedWidth(panelW);
    m_panel->move(-panelW, 0);

    m_listView->setModel(m_model);
    m_listView->setItemDelegate(new Md3NavDelegate(this));
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    enableTouchScroll(m_listView);
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setSpacing(2);

    auto *panelLayout = new QVBoxLayout(m_panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);
    panelLayout->addWidget(m_listView, 1);  // stretch=1: volle verbleibende Höhe

    connect(m_listView, &QListView::clicked,
            this, [this](const QModelIndex &idx) {
                if (idx.data(Qt::UserRole + 2).toBool()) return; // Separator ignorieren
                setCurrentIndex(idx.row());
                close();
                emit pageSelected(idx.row());
            });

    hide();
}

void DrawerWidget::addPage(const QString &icon, const QString &title)
{
    auto *item = new QStandardItem(title);
    item->setData(icon,  Qt::UserRole);
    item->setData(false, Qt::UserRole + 2);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_model->appendRow(item);
}

void DrawerWidget::addSeparator()
{
    auto *item = new QStandardItem(QString());
    item->setData(QString(), Qt::UserRole);
    item->setData(true, Qt::UserRole + 2);  // isSeparator
    item->setFlags(Qt::NoItemFlags);        // nicht klickbar/selektierbar
    m_model->appendRow(item);
}

void DrawerWidget::setCurrentIndex(int index)
{
    m_currentIndex = index;
    m_listView->setCurrentIndex(m_model->index(index, 0));
    m_model->layoutChanged();
}

int DrawerWidget::currentIndex() const { return m_currentIndex; }

void DrawerWidget::open()
{
    if (m_open) return;
    m_open = true;

    QWidget *p = parentWidget();
    if (!p) return;

    // Overlay: volle Größe des DrawerWidgets (das selbst schon bei y=56 beginnt)
    m_overlay->setGeometry(0, 0, width(), height());
    m_overlay->setStyleSheet("background:rgba(0,0,0,0);");
    m_overlay->show();
    m_overlay->raise();

    show();
    raise();

    // Panel: volle Höhe des DrawerWidgets
    const int pW = m_panel->width();
    m_panel->setGeometry(-pW, 0, pW, height());
    m_panel->show();
    m_panel->raise();

    auto *anim = new QPropertyAnimation(m_panel, "pos", this);
    anim->setDuration(250);
    anim->setStartValue(QPoint(-pW, 0));
    anim->setEndValue(QPoint(0, 0));
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    m_overlay->setStyleSheet("background:rgba(0,0,0,50);");
}

void DrawerWidget::close()
{
    if (!m_open) return;
    m_open = false;

    auto *anim = new QPropertyAnimation(m_panel, "pos", this);
    anim->setDuration(200);
    anim->setStartValue(QPoint(0, 0));
    anim->setEndValue(QPoint(-m_panel->width(), 0));
    anim->setEasingCurve(QEasingCurve::InCubic);
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        m_overlay->hide();
        hide();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

bool DrawerWidget::isOpen() const { return m_open; }

bool DrawerWidget::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_overlay &&
        (ev->type() == QEvent::MouseButtonPress ||
         ev->type() == QEvent::TouchBegin)) {
        close();
        return true;
    }
    return QWidget::eventFilter(obj, ev);
}

void DrawerWidget::resizeEvent(QResizeEvent *ev)
{
    QWidget *p = parentWidget();
    if (p) {
        m_overlay->setGeometry(0, 0, width(), height());
        m_panel->setGeometry(m_open ? 0 : -m_panel->width(), 0, m_panel->width(), height());
    }
    QWidget::resizeEvent(ev);
}

void DrawerWidget::setItemVisible(int index, bool visible)
{
    QStandardItem *it = m_model->item(index);
    if (!it) return;
    if (visible) {
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        it->setData(false, Qt::UserRole + 1);
    } else {
        it->setFlags(Qt::NoItemFlags);
        it->setData(true, Qt::UserRole + 1);
    }
    m_listView->setRowHidden(index, !visible);
}
