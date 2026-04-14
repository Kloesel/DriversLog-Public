#pragma once
#include <QWidget>
#include <QPropertyAnimation>
#include <QListView>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QLabel>

class DrawerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DrawerWidget(QWidget *parent = nullptr);

    void addPage(const QString &icon, const QString &title);
    void addSeparator();           // fügt eine nicht-selektierbare Trennlinie ein
    void toggle()      { m_open ? close() : open(); }
    void closeDrawer() { close(); }
    bool isOpen() const;

    void setCurrentIndex(int index);
    int  currentIndex() const;

    void setItemVisible(int index, bool visible);

signals:
    void pageSelected(int index);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void open();
    void close();

    QWidget            *m_overlay;
    QWidget            *m_panel;
    QListView          *m_listView;
    QStandardItemModel *m_model;
    int                 m_currentIndex;
    bool                m_open    = false;
};
