#pragma once

#include <QtCore/QCoreApplication>

class Application : public QCoreApplication
{
    Q_OBJECT;

public:
    using QCoreApplication::QCoreApplication;

public slots:
    void onRun();
};