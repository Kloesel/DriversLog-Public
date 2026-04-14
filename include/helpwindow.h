#pragma once
#include <QWidget>
#include <QDialog>
#include "version.h"

class HelpWindow : public QDialog
{
    Q_OBJECT
public:
    explicit HelpWindow(QWidget *parent = nullptr);
};
