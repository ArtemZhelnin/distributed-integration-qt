#pragma once

#include <QtGlobal>
#include <QDataStream>
#include <QString>

namespace netproj {

/**
 * @brief Protocol magic value used to validate frames.
 */
static constexpr quint32 kProtocolMagic = 0x4E50524A; // 'NPRJ'

/**
 * @brief Protocol version.
 */
static constexpr quint16 kProtocolVersion = 1;

/**
 * @brief Message types supported by the wire protocol.
 */
enum class MessageType : quint8 {
    Hello = 1,
    Task = 2,
    Result = 3,
    Error = 4
};

/**
 * @brief Numerical integration methods.
 */
enum class MethodType : quint8 {
    MidpointRectangles = 1,
    Trapezoids = 2,
    Simpson = 3
};

/**
 * @brief Client greeting containing number of available CPU cores.
 */
struct HelloMsg {
    quint32 cores = 0;
};

/**
 * @brief Integration task sent from server to client.
 */
struct TaskMsg {
    double a = 0.0;
    double b = 0.0;
    double h = 0.0;
    MethodType method = MethodType::Simpson;
    quint32 clientIndex = 0;
    quint32 clientCount = 0;
};

/**
 * @brief Client computation result.
 */
struct ResultMsg {
    double value = 0.0;
};

/**
 * @brief Error message for reporting failures.
 */
struct ErrorMsg {
    QString text;
};

/**
 * @brief Message envelope present at the beginning of each payload.
 */
struct Envelope {
    quint32 magic = kProtocolMagic;
    quint16 version = kProtocolVersion;
    MessageType type = MessageType::Error;
};

/**
 * @brief Serialize Envelope to QDataStream.
 */
inline QDataStream &operator<<(QDataStream &out, const Envelope &e) {
    out << e.magic << e.version << static_cast<quint8>(e.type);
    return out;
}

/**
 * @brief Deserialize Envelope from QDataStream.
 */
inline QDataStream &operator>>(QDataStream &in, Envelope &e) {
    quint8 t = 0;
    in >> e.magic >> e.version >> t;
    e.type = static_cast<MessageType>(t);
    return in;
}

/**
 * @brief Serialize HelloMsg to QDataStream.
 */
inline QDataStream &operator<<(QDataStream &out, const HelloMsg &m) {
    out << m.cores;
    return out;
}

/**
 * @brief Deserialize HelloMsg from QDataStream.
 */
inline QDataStream &operator>>(QDataStream &in, HelloMsg &m) {
    in >> m.cores;
    return in;
}

/**
 * @brief Serialize TaskMsg to QDataStream.
 */
inline QDataStream &operator<<(QDataStream &out, const TaskMsg &m) {
    out << m.a << m.b << m.h << static_cast<quint8>(m.method) << m.clientIndex << m.clientCount;
    return out;
}

/**
 * @brief Deserialize TaskMsg from QDataStream.
 */
inline QDataStream &operator>>(QDataStream &in, TaskMsg &m) {
    quint8 method = 0;
    in >> m.a >> m.b >> m.h >> method >> m.clientIndex >> m.clientCount;
    m.method = static_cast<MethodType>(method);
    return in;
}

/**
 * @brief Serialize ResultMsg to QDataStream.
 */
inline QDataStream &operator<<(QDataStream &out, const ResultMsg &m) {
    out << m.value;
    return out;
}

/**
 * @brief Deserialize ResultMsg from QDataStream.
 */
inline QDataStream &operator>>(QDataStream &in, ResultMsg &m) {
    in >> m.value;
    return in;
}

/**
 * @brief Serialize ErrorMsg to QDataStream.
 */
inline QDataStream &operator<<(QDataStream &out, const ErrorMsg &m) {
    out << m.text;
    return out;
}

/**
 * @brief Deserialize ErrorMsg from QDataStream.
 */
inline QDataStream &operator>>(QDataStream &in, ErrorMsg &m) {
    in >> m.text;
    return in;
}

} // namespace netproj
