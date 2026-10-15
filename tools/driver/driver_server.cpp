#include "driver_server.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "selector.h"
#include "ui_driver.h"

namespace ksv::driver {
    DriverServer::DriverServer(UiDriver &driver, QObject *parent)
        : QObject(parent), m_driver(driver) {
        connect(&m_server, &QTcpServer::newConnection, this, &DriverServer::onNewConnection);
    }

    bool DriverServer::listen(const quint16 port) {
        return m_server.listen(QHostAddress::LocalHost, port);
    }

    void DriverServer::onNewConnection() {
        while (QTcpSocket *socket = m_server.nextPendingConnection()) {
            m_buffers.insert(socket, {});
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                m_buffers[socket].append(socket->readAll());
                processPending();
            });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                m_buffers.remove(socket);
                socket->deleteLater();
            });
        }
    }

    void DriverServer::processPending() {
        if (m_busy) return;  // the running command's event-loop spin delivered more data
        m_busy = true;
        bool quitRequested = false;
        bool progressed = true;
        while (progressed && !quitRequested) {
            progressed = false;
            for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
                const auto newline = it.value().indexOf('\n');
                if (newline < 0) continue;
                QPointer<QTcpSocket> socket = it.key();
                const QByteArray line = it.value().left(newline);
                it.value().remove(0, newline + 1);
                const QByteArray response = handleLine(line.trimmed(), quitRequested);
                if (socket && socket->state() == QAbstractSocket::ConnectedState) {
                    socket->write(response);
                    socket->write("\n");
                    socket->flush();
                }
                progressed = true;
                break;  // the command may have changed m_buffers; restart the scan
            }
        }
        m_busy = false;
        if (quitRequested) QTimer::singleShot(50, qApp, &QCoreApplication::quit);
    }

    QByteArray DriverServer::handleLine(const QByteArray &line, bool &quitRequested) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        if (!document.isObject()) {
            return QJsonDocument(QJsonObject{{"ok", false},
                                             {"error", QStringLiteral("invalid JSON request: %1")
                                                           .arg(parseError.errorString())}})
                .toJson(QJsonDocument::Compact);
        }
        const QJsonObject request = document.object();
        QJsonObject response{{"id", request.value("id")}};
        const QString command = request.value("cmd").toString();
        if (command == u"quit") {
            quitRequested = true;
            response.insert("ok", true);
            response.insert("result", QJsonObject{{"quitting", true}});
            return QJsonDocument(response).toJson(QJsonDocument::Compact);
        }
        try {
            response.insert("result", m_driver.execute(command, request.value("args").toObject()));
            response.insert("ok", true);
        } catch (const DriverError &error) {
            response.insert("ok", false);
            response.insert("error", error.message());
            if (!error.details().isEmpty()) response.insert("details", error.details());
        } catch (const std::exception &error) {
            response.insert("ok", false);
            response.insert("error", QString::fromUtf8(error.what()));
        }
        return QJsonDocument(response).toJson(QJsonDocument::Compact);
    }
}
