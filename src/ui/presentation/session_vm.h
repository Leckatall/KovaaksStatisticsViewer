//
// Created by Lecka on 30/07/2026.
//

#ifndef KOVAAKSSTATSVIEWER_SESSION_VM_H
#define KOVAAKSSTATSVIEWER_SESSION_VM_H
#include <QObject>
#include <qqmlintegration.h>

namespace ksv::presentation {
    class SessionViewModel: public QObject {
        Q_OBJECT
        QML_INTERFACE
    };
} // ksv

#endif //KOVAAKSSTATSVIEWER_SESSION_VM_H
