#pragma once

#include <QtWidgets/QApplication>

class Application : public QApplication
{
    Q_OBJECT;

public:
    using QApplication::QApplication;

public slots:
    void onRun();
};