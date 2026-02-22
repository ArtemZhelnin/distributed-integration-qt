#include "../common/framed_socket.h"
#include "../common/integrator.h"
#include "../common/message_io.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QIODevice>
#include <QTextStream>
#include <QTcpServer>
#include <QTcpSocket>

#include <QRegularExpression>
#include <QStringList>

namespace netproj {

/**
 * @brief Per-client server-side state.
 */
struct ClientState {
    FramedSocket *framed = nullptr;
    quint32 cores = 0;
    bool helloReceived = false;
    bool resultReceived = false;
    double result = 0.0;
};

/**
 * @brief Parse method id from CLI input.
 */
static MethodType parseMethod(int v) {
    switch (v) {
    case 1:
        return MethodType::MidpointRectangles;
    case 2:
        return MethodType::Trapezoids;
    case 3:
        return MethodType::Simpson;
    default:
        return MethodType::Simpson;
    }
}

/**
 * @brief Get method name for logging.
 */
static QString methodName(MethodType m) {
    switch (m) {
    case MethodType::MidpointRectangles:
        return "midpoint_rectangles";
    case MethodType::Trapezoids:
        return "trapezoids";
    case MethodType::Simpson:
        return "simpson";
    default:
        return "unknown";
    }
}

/**
 * @brief TCP server application. Accepts N clients, distributes integration tasks proportionally to core counts,
 * and reduces partial results.
 */
class ServerApp : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Construct server app.
     */
    explicit ServerApp(QObject *parent = nullptr)
        : QObject(parent) {
        connect(&m_server, &QTcpServer::newConnection, this, &ServerApp::onNewConnection);
    }

    /**
     * @brief Enable/disable pause on finish (wait for Enter before exiting).
     */
    void setPauseOnFinish(bool v) { m_pauseOnFinish = v; }

    /**
     * @brief Start listening on port and set expected client count.
     */
    bool start(quint16 port, int expectedClients) {
        m_expectedClients = expectedClients;
        if (!m_server.listen(QHostAddress::Any, port)) {
            qCritical() << "Server listen failed:" << m_server.errorString();
            return false;
        }
        qInfo() << "Server listening on port" << port << ", expecting" << expectedClients << "clients";
        return true;
    }

    /**
     * @brief Set integration task parameters.
     */
    void setTask(double a, double b, double h, MethodType method) {
        m_a = a;
        m_b = b;
        m_h = h;
        m_method = method;
    }

private slots:
    /**
     * @brief Accept incoming TCP connections.
     */
    void onNewConnection() {
        while (QTcpSocket *sock = m_server.nextPendingConnection()) {
            sock->setSocketOption(QAbstractSocket::LowDelayOption, 1);

            auto *framed = new FramedSocket(sock, sock);
            ClientState st;
            st.framed = framed;
            m_clients.push_back(st);
            const int idx = m_clients.size() - 1;

            qInfo() << "Client connected" << sock->peerAddress().toString() << ":" << sock->peerPort();

            connect(framed, &FramedSocket::frameReceived, this, [this, idx](const QByteArray &payload) {
                onFrame(idx, payload);
            });
            connect(framed, &FramedSocket::disconnected, this, [this, idx]() {
                onClientDisconnected(idx);
            });

            if (m_clients.size() == static_cast<size_t>(m_expectedClients)) {
                qInfo() << "All clients connected. Waiting for HELLO from each client...";
            }
        }
    }

private:
    /**
     * @brief Disconnection handler.
     */
    void onClientDisconnected(int idx) {
        qWarning() << "Client disconnected idx=" << idx;
    }

    /**
     * @brief Handle incoming framed payload from a client.
     */
    void onFrame(int idx, const QByteArray &payload) {
        const auto pm = parseMessage(payload);
        if (!pm.ok) {
            qWarning() << "Failed to parse message from client" << idx << ":" << pm.parseError;
            return;
        }

        auto &c = m_clients[static_cast<size_t>(idx)];

        if (pm.env.type == MessageType::Hello) {
            c.helloReceived = true;
            c.cores = pm.hello.cores;
            qInfo() << "HELLO from client" << idx << ", cores=" << c.cores;
            maybeDispatchTasks();
            return;
        }

        if (pm.env.type == MessageType::Result) {
            c.resultReceived = true;
            c.result = pm.result.value;
            qInfo() << "RESULT from client" << idx << ":" << c.result;
            maybeFinalize();
            return;
        }

        if (pm.env.type == MessageType::Error) {
            qWarning() << "ERROR from client" << idx << ":" << pm.error.text;
            c.resultReceived = true;
            c.result = 0.0;
            maybeFinalize();
            return;
        }

        qWarning() << "Unexpected message type from client" << idx;
    }

    /**
     * @brief Dispatch tasks when all clients are connected and HELLO is received.
     */
    void maybeDispatchTasks() {
        if (m_dispatched) {
            return;
        }
        if (m_expectedClients <= 0) {
            return;
        }
        if (m_clients.size() != static_cast<size_t>(m_expectedClients)) {
            return;
        }
        for (const auto &c : m_clients) {
            if (!c.helloReceived) {
                return;
            }
        }

        quint64 totalCores = 0;
        for (const auto &c : m_clients) {
            totalCores += std::max<quint32>(1u, c.cores);
        }

        qInfo() << "Dispatching tasks. Total cores=" << totalCores << ", method=" << methodName(m_method)
                << ", interval=[" << m_a << "," << m_b << "], h=" << m_h;

        const double len = (m_b - m_a);
        double cursor = m_a;

        for (size_t i = 0; i < m_clients.size(); ++i) {
            const quint32 cores = std::max<quint32>(1u, m_clients[i].cores);
            const double frac = static_cast<double>(cores) / static_cast<double>(totalCores);
            const double partLen = len * frac;
            const double aPart = cursor;
            const double bPart = (i + 1 == m_clients.size()) ? m_b : (cursor + partLen);
            cursor = bPart;

            TaskMsg t;
            t.a = aPart;
            t.b = bPart;
            t.h = m_h;
            t.method = m_method;
            t.clientIndex = static_cast<quint32>(i);
            t.clientCount = static_cast<quint32>(m_clients.size());

            m_clients[i].framed->sendFrame(serializeTask(t));
            qInfo() << "Sent TASK to client" << static_cast<int>(i) << ": [" << aPart << "," << bPart << "]";
        }

        m_timer.start();
        m_dispatched = true;
    }

    /**
     * @brief Finalize reduction when all partial results are received.
     */
    void maybeFinalize() {
        if (!m_dispatched || m_finished) {
            return;
        }

        for (const auto &c : m_clients) {
            if (!c.resultReceived) {
                return;
            }
        }

        double sum = 0.0;
        for (const auto &c : m_clients) {
            sum += c.result;
        }

        const qint64 ms = m_timer.elapsed();
        qInfo() << "FINAL RESULT:" << sum << ", time=" << ms << "ms";
        m_finished = true;

        if (m_pauseOnFinish) {
            QTextStream in(stdin);
            QTextStream out(stdout);
            out << "Press Enter to exit..." << Qt::endl;
            in.readLine();
        }

        QCoreApplication::quit();
    }

    QTcpServer m_server;
    QVector<ClientState> m_clients;
    int m_expectedClients = 0;

    double m_a = 2.0;
    double m_b = 10.0;
    double m_h = 1e-4;
    MethodType m_method = MethodType::Simpson;

    bool m_dispatched = false;
    bool m_finished = false;
    QElapsedTimer m_timer;

    bool m_pauseOnFinish = false;
};

} // namespace netproj

#include "server_main.moc"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QTextStream in(stdin);
    QTextStream out(stdout);

    const QStringList args = QCoreApplication::arguments();
    const bool pause = args.contains("--pause");

    out << "Enter port: " << Qt::flush;
    const QString portLine = in.readLine().trimmed();
    bool ok = false;
    const quint16 port = static_cast<quint16>(portLine.toUShort(&ok));
    if (!ok || port == 0) {
        qCritical() << "Invalid port";
        return 1;
    }

    out << "Enter expected client count N: " << Qt::flush;
    const QString nLine = in.readLine().trimmed();
    const int n = nLine.toInt(&ok);
    if (!ok || n <= 0) {
        qCritical() << "Invalid client count";
        return 1;
    }

    out << "Enter A B h method(1=mid,2=trap,3=simp): " << Qt::flush;
    const QString paramsLine = in.readLine().trimmed();
    const QStringList parts = paramsLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 4) {
        qCritical() << "Invalid parameters line";
        return 1;
    }

    const double a = parts[0].toDouble(&ok);
    if (!ok) {
        qCritical() << "Invalid A";
        return 1;
    }
    const double b = parts[1].toDouble(&ok);
    if (!ok) {
        qCritical() << "Invalid B";
        return 1;
    }
    const double h = parts[2].toDouble(&ok);
    if (!ok || !(h > 0.0)) {
        qCritical() << "Invalid step h";
        return 1;
    }
    const int method = parts[3].toInt(&ok);
    if (!ok) {
        qCritical() << "Invalid method";
        return 1;
    }

    netproj::ServerApp srv;
    srv.setPauseOnFinish(pause);
    srv.setTask(a, b, h, netproj::parseMethod(method));

    if (!srv.start(port, n)) {
        return 1;
    }

    return app.exec();
}
