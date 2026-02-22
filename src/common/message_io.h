#pragma once

#include "protocol.h"

#include <QByteArray>
#include <QDataStream>

namespace netproj {

/**
 * @brief Serialize HelloMsg into payload bytes (Envelope + message body).
 */
inline QByteArray serializeHello(const HelloMsg &m) {
    QByteArray buf;
    QDataStream out(&buf, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);
    Envelope e;
    e.type = MessageType::Hello;
    out << e << m;
    return buf;
}

/**
 * @brief Serialize TaskMsg into payload bytes (Envelope + message body).
 */
inline QByteArray serializeTask(const TaskMsg &m) {
    QByteArray buf;
    QDataStream out(&buf, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);
    Envelope e;
    e.type = MessageType::Task;
    out << e << m;
    return buf;
}

/**
 * @brief Serialize ResultMsg into payload bytes (Envelope + message body).
 */
inline QByteArray serializeResult(const ResultMsg &m) {
    QByteArray buf;
    QDataStream out(&buf, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);
    Envelope e;
    e.type = MessageType::Result;
    out << e << m;
    return buf;
}

/**
 * @brief Serialize ErrorMsg into payload bytes (Envelope + message body).
 */
inline QByteArray serializeError(const ErrorMsg &m) {
    QByteArray buf;
    QDataStream out(&buf, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);
    Envelope e;
    e.type = MessageType::Error;
    out << e << m;
    return buf;
}

/**
 * @brief Parsed message container for a payload.
 */
struct ParsedMessage {
    Envelope env;
    HelloMsg hello;
    TaskMsg task;
    ResultMsg result;
    ErrorMsg error;
    bool ok = false;
    QString parseError;
};

/**
 * @brief Parse a payload buffer into a typed message.
 *
 * @param buf Raw payload bytes (must start with Envelope).
 * @return ParsedMessage with ok flag and parseError in case of failure.
 */
inline ParsedMessage parseMessage(const QByteArray &buf) {
    ParsedMessage pm;

    QDataStream in(buf);
    in.setVersion(QDataStream::Qt_6_5);

    in >> pm.env;
    if (in.status() != QDataStream::Ok) {
        pm.parseError = "QDataStream status not OK after reading envelope";
        return pm;
    }

    if (pm.env.magic != kProtocolMagic || pm.env.version != kProtocolVersion) {
        pm.parseError = "Protocol magic/version mismatch";
        return pm;
    }

    switch (pm.env.type) {
    case MessageType::Hello:
        in >> pm.hello;
        break;
    case MessageType::Task:
        in >> pm.task;
        break;
    case MessageType::Result:
        in >> pm.result;
        break;
    case MessageType::Error:
        in >> pm.error;
        break;
    default:
        pm.parseError = "Unknown message type";
        return pm;
    }

    if (in.status() != QDataStream::Ok) {
        pm.parseError = "QDataStream status not OK after reading message body";
        return pm;
    }

    pm.ok = true;
    return pm;
}

} // namespace netproj
