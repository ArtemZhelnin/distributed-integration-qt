#pragma once

#include <QObject>
#include <QTcpSocket>

namespace netproj {

/**
 * @brief Small helper around QTcpSocket that implements length-prefixed framing.
 *
 * Each frame is encoded as:
 * - 4 bytes (quint32, QDataStream) payload size
 * - payload bytes
 */
class FramedSocket : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Construct a framed socket wrapper.
     * @param socket Connected TCP socket.
     * @param parent QObject parent.
     */
    explicit FramedSocket(QTcpSocket *socket, QObject *parent = nullptr);

    /**
     * @brief Access underlying socket.
     */
    QTcpSocket *socket() const { return m_socket; }

    /**
     * @brief Send one framed payload.
     */
    void sendFrame(const QByteArray &payload);

signals:
    /**
     * @brief Emitted when a full payload frame has been received.
     */
    void frameReceived(const QByteArray &payload);

    /**
     * @brief Emitted when the underlying socket is disconnected.
     */
    void disconnected();

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
    quint32 m_expectedSize = 0;

    bool tryConsumeOneFrame();
};

} // namespace netproj
