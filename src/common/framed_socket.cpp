#include "framed_socket.h"

#include <QDataStream>

namespace netproj {

FramedSocket::FramedSocket(QTcpSocket *socket, QObject *parent)
    : QObject(parent), m_socket(socket) {
    Q_ASSERT(m_socket);

    connect(m_socket, &QTcpSocket::readyRead, this, &FramedSocket::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &FramedSocket::onDisconnected);
}

void FramedSocket::sendFrame(const QByteArray &payload) {
    QByteArray frame;
    frame.reserve(static_cast<int>(sizeof(quint32) + payload.size()));

    QDataStream out(&frame, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);
    out << static_cast<quint32>(payload.size());
    frame.append(payload);

    m_socket->write(frame);
    m_socket->flush();
}

void FramedSocket::onReadyRead() {
    m_buffer.append(m_socket->readAll());
    while (tryConsumeOneFrame()) {
    }
}

void FramedSocket::onDisconnected() {
    emit disconnected();
}

bool FramedSocket::tryConsumeOneFrame() {
    if (m_expectedSize == 0) {
        if (m_buffer.size() < static_cast<int>(sizeof(quint32))) {
            return false;
        }

        QDataStream in(m_buffer);
        in.setVersion(QDataStream::Qt_6_5);
        in >> m_expectedSize;

        m_buffer.remove(0, static_cast<int>(sizeof(quint32)));
    }

    if (m_buffer.size() < static_cast<int>(m_expectedSize)) {
        return false;
    }

    QByteArray payload = m_buffer.left(static_cast<int>(m_expectedSize));
    m_buffer.remove(0, static_cast<int>(m_expectedSize));
    m_expectedSize = 0;

    emit frameReceived(payload);
    return true;
}

} // namespace netproj
