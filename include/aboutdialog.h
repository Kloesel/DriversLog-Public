#pragma once
#include <QDialog>
#include <QWidget>
#include "version.h"

class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
};
