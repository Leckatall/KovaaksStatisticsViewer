#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>

namespace ksv::driver {
    class UiDriver;

    // Newline-delimited JSON over a 127.0.0.1-only TCP socket.
    //   request:  {"id": <any>, "cmd": "<command>", "args": {...}}
    //   response: {"id": <same>, "ok": true, "result": {...}}
    //          or {"id": <same>, "ok": false, "error": "<message>", "details": {...}}
    // Commands run one at a time on the GUI thread; a command that waits (input
    // settling, `wait`) spins the event loop, so requests arriving meanwhile are
    // queued rather than run re-entrantly.
    class DriverServer final : public QObject {
        Q_OBJECT
    public:
        explicit DriverServer(UiDriver &driver, QObject *parent = nullptr);

        bool listen(quint16 port = 0);
        [[nodiscard]] quint16 port() const { return m_server.serverPort(); }

    private:
        void onNewConnection();
        void processPending();
        QByteArray handleLine(const QByteArray &line, bool &quitRequested);

        UiDriver &m_driver;
        QTcpServer m_server;
        QHash<QTcpSocket *, QByteArray> m_buffers;
        bool m_busy = false;
    };
}
