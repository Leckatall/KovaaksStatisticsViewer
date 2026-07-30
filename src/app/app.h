//
// Created by Lecka on 27/07/2026.
//

#ifndef KOVAAKSSTATISTICSVIEWER_APP_H
#define KOVAAKSSTATISTICSVIEWER_APP_H

#include <QMainWindow>
#include <QFrame>
#include <QGridLayout>

namespace ksv::application {
    class App: public QObject {
        Q_OBJECT
    public:
        App();
        void start() const;

    private:
        void initLayout() const;
        // void initConnections();
        // void setStatusBarMessage() const;

        QMainWindow *m_window;
        QFrame *m_container;
    };
} // Application

#endif //KOVAAKSSTATISTICSVIEWER_APP_H