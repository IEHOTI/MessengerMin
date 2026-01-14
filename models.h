#ifndef MODELS_H
#define MODELS_H

#include <QString>
#include <QDateTime>

struct PeerInfo {
    QString name;
    QString ip;
    quint16 tcpPort;
    QDateTime lastDiscover;
};

struct ChatMessage {
    QString sender;
    QString text;
    QDateTime timestamp;
    bool isOutgoing;
    QString filePath;      // Путь к локальному файлу
    QString fileName;      // Имя файла для отображения
    qint64 fileSize;       // Размер файла ?
    QByteArray fileData;   // Данные файла (для маленьких файлов) ?
    bool isFile;           // Флаг, что это файл
};

struct FileTransferInfo {
    QString transferId;
    qint64 fileSize;
    int totalParts;
    int nextPart;
};

enum ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Listening
};

enum NotificationType {
    Error,
    Warning,
    Information,
    Question
};

#endif // MODELS_H
