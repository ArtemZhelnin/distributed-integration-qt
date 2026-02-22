#include "../common/framed_socket.h"
#include "../common/integrator.h"
#include "../common/message_io.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTextStream>
#include <QTcpSocket>
#include <QtConcurrent/QtConcurrent>

#include <vector>
#include <exception>

namespace netproj {

static double integrateChunk(double a, double b, double h, MethodType method) {
    return Integrator::integrate(a, b, h, method);
}

/**
 * @brief Client application that connects to server, computes assigned integral chunk and sends result back.
 */
class ClientApp : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Construct client app.
     */
    explicit ClientApp(QObject *parent = nullptr)
        : QObject(parent) {
        connect(&m_socket, &QTcpSocket::connected, this, &ClientApp::onConnected);
        connect(&m_socket, &QTcpSocket::errorOccurred, this, &ClientApp::onError);
    }

    /**
     * @brief Connect to server by host and port.
     */
    void connectTo(const QString &host, quint16 port) {
        qInfo() << "Connecting to" << host << ":" << port;
        if (host.trimmed().isEmpty() || port == 0) {
            qCritical() << "Invalid host/port";
            QCoreApplication::quit();
            return;
        }
        m_socket.connectToHost(host, port);
    }

private slots:
    /**
     * @brief TCP connected handler. Sends HELLO message with core count.
     */
    void onConnected() {
        qInfo() << "Connected";
        m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);

        m_framed = new FramedSocket(&m_socket, this);
        connect(m_framed, &FramedSocket::frameReceived, this, &ClientApp::onFrame);
        connect(m_framed, &FramedSocket::disconnected, this, &ClientApp::onDisconnected);

        const quint32 cores = static_cast<quint32>(std::max(1, QThread::idealThreadCount()));
        HelloMsg hello;
        hello.cores = cores;

        m_framed->sendFrame(serializeHello(hello));
        qInfo() << "Sent HELLO, cores=" << cores;
    }

    /**
     * @brief Frame handler (TASK, ERROR).
     */
    void onFrame(const QByteArray &payload) {
        const auto pm = parseMessage(payload);
        if (!pm.ok) {
            qWarning() << "Failed to parse server message:" << pm.parseError;
            return;
        }

        if (pm.env.type == MessageType::Task) {
            qInfo() << "TASK received:" << pm.task.a << pm.task.b << "h=" << pm.task.h;
            computeAndSend(pm.task);
            return;
        }

        if (pm.env.type == MessageType::Error) {
            qWarning() << "Server ERROR:" << pm.error.text;
            QCoreApplication::quit();
            return;
        }

        qWarning() << "Unexpected message type from server";
    }

    /**
     * @brief Disconnection handler.
     */
    void onDisconnected() {
        qWarning() << "Disconnected";
        QCoreApplication::quit();
    }

    /**
     * @brief Socket error handler.
     */
    void onError(QAbstractSocket::SocketError) {
        qCritical() << "Socket error:" << m_socket.errorString();
    }

private:
    /**
     * @brief Compute assigned integral task using multiple CPU cores and send result.
     */
    void computeAndSend(const TaskMsg &task) {
        try {
            const int threads = std::max(1, QThread::idealThreadCount());

            const double len = task.b - task.a;
            const double part = len / static_cast<double>(threads);

            std::vector<QFuture<double>> futures;
            futures.reserve(static_cast<size_t>(threads));

            QElapsedTimer timer;
            timer.start();

            for (int i = 0; i < threads; ++i) {
                const double a = task.a + static_cast<double>(i) * part;
                const double b = (i + 1 == threads) ? task.b : (a + part);
                futures.push_back(QtConcurrent::run(&integrateChunk, a, b, task.h, task.method));
            }

            double sum = 0.0;
            for (auto &f : futures) {
                f.waitForFinished();
                sum += f.result();
            }

            const qint64 ms = timer.elapsed();
            qInfo() << "Computed local sum=" << sum << ", time=" << ms << "ms";

            ResultMsg r;
            r.value = sum;
            m_framed->sendFrame(serializeResult(r));
            qInfo() << "Sent RESULT";
        } catch (const std::exception &e) {
            qCritical() << "Computation failed:" << e.what();
            ErrorMsg err;
            err.text = QString::fromUtf8(e.what());
            m_framed->sendFrame(serializeError(err));
        } catch (...) {
            qCritical() << "Computation failed: unknown exception";
            ErrorMsg err;
            err.text = "unknown exception";
            m_framed->sendFrame(serializeError(err));
        }

        m_socket.disconnectFromHost();
    }

    QTcpSocket m_socket;
    FramedSocket *m_framed = nullptr;
};

} // namespace netproj

#include "client_main.moc"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QTextStream in(stdin);
    QTextStream out(stdout);

    const QStringList args = QCoreApplication::arguments();
    const bool pause = args.contains("--pause");

    QString host;
    quint16 port = 0;

    const int hostIdx = args.indexOf("--host");
    if (hostIdx >= 0 && hostIdx + 1 < args.size()) {
        host = args[hostIdx + 1];
    }
    const int portIdx = args.indexOf("--port");
    if (portIdx >= 0 && portIdx + 1 < args.size()) {
        bool ok = false;
        port = static_cast<quint16>(args[portIdx + 1].toUShort(&ok));
        if (!ok) {
            port = 0;
        }
    }

    if (host.trimmed().isEmpty()) {
        out << "Enter server host: " << Qt::flush;
        host = in.readLine().trimmed();
    }
    if (port == 0) {
        out << "Enter server port: " << Qt::flush;
        const QString portLine = in.readLine().trimmed();
        bool ok = false;
        port = static_cast<quint16>(portLine.toUShort(&ok));
        if (!ok) {
            qCritical() << "Invalid port input";
            return 1;
        }
    }

    netproj::ClientApp client;
    client.connectTo(host, port);

    const int rc = app.exec();
    if (pause) {
        out << "Press Enter to exit..." << Qt::endl;
        in.readLine();
    }
    return rc;
}
