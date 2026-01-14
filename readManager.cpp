#include "config.h"
#include "mainwindow.h"
#include <QUdpSocket>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QDir>
#include <QMessageBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QCryptographicHash>
#include <QTimer>

void MainWindow::onReadyReadUDP()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(udpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        udpSocket->readDatagram(datagram.data(), datagram.size(),
                                &sender, &senderPort);

        QString ip;

        if (sender.toString().startsWith("::ffff:"))
            ip = sender.toString().mid(7); // "::ffff:192.168.1.1" -> "192.168.1.1"
        else if (sender.protocol() == QHostAddress::IPv4Protocol)
            ip = sender.toString();
        else {
            qDebug() << "Пропускаем IPv6:" << sender.toString();
            continue;
        }
        if (ip.isEmpty()) continue;

        QList<QHostAddress> localAddresses = QNetworkInterface::allAddresses();
        bool isMyAddress = false;

        for (const QHostAddress &localAddr : localAddresses) {
            if (localAddr.protocol() == QAbstractSocket::IPv4Protocol
                && sender.protocol() == QAbstractSocket::IPv4Protocol
                && localAddr.toString() == sender.toString()) {
                isMyAddress = true;
                break;
            }
            if (sender.toString().startsWith("::ffff:")) {
                QString senderIPv4 = sender.toString().mid(7);
                if (localAddr.toString() == senderIPv4) {
                    isMyAddress = true;
                    break;
                }
            }
        }
        if (isMyAddress) continue;

        QJsonDocument doc = QJsonDocument::fromJson(datagram);
        if (doc.isNull()) continue;

        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();

        if (type == "discovery") {
            QString name = obj["name"].toString();
            quint16 peerTcpPort = obj["tcp_port"].toInt();
            addPeer(name, ip, peerTcpPort);
        }
        else continue;
    }
}

void MainWindow::onReadyReadTCP() {
    if (!tcpSocket) return;
    qDebug() << "Bytes available:" << tcpSocket->bytesAvailable();
    QByteArray data = tcpSocket->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull()) {
        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();

        qDebug() << "Message type:" << type;

        if (type == "message") {
            QString text = obj["text"].toString();
            QString sender = obj["sender"].toString();
            emit addMessageToHistory(sender, text, false);
        }
        else if (type == "connect") {
            emit successConnect();
        }
        else if (type == "file_info") {
            qDebug() << "FILE_INFO received!";
            qDebug() << "File:" << obj["file_name"].toString();
            qDebug() << "Size:" << obj["file_size"].toString();
            receiveFile(obj);
        }
        else if (type == "file_next") {
            qDebug() << "next chunk";
            qDebug() << obj;
            sendNextChunk(obj);
        }
        else if (type == "file_data") {
            QString transferId = obj["transfer_id"].toString();
            int part = obj["chunk"].toInt();
            qDebug() << "FILE_DATA:" << transferId << " part " << part;
            processNextChunk(obj);
        }
        else if (type == "file_done") {
            qDebug() << "file received";
            successReceiveFile();
        }
        else if (type == "file_error") {
            qDebug() << "FILE_ERROR:" << obj["reason"].toString();
            showNotification(NotificationType::Error,
                             QString("Ошибка передачи файла: %1").arg(obj["reason"].toString()));
        }
        else {
            qDebug() << "Unknown message type:" << type;
        }
    }
}
