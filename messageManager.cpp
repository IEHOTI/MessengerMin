#include "mainwindow.h"
#include <QTcpSocket>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>

void MainWindow::sendMessage(const QString &text) {
    if (!tcpSocket || !tcpSocket->isOpen()) {
        emit lostConnect();
        return;
    }

    QJsonObject message;
    message["type"] = "message";
    message["text"] = text;
    message["sender"] = localName;
    message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonDocument doc(message);
    if (tcpSocket && tcpSocket->state() == QAbstractSocket::ConnectedState) {
        tcpSocket->write(doc.toJson());
        tcpSocket->flush();
    } else {
        emit lostConnect();
        return;
    }
    emit addMessageToHistory(localName, text, true, false);
}

void MainWindow::sendConfirmConnection() {
    if (!tcpSocket || !tcpSocket->isOpen()) {
        emit lostConnect();
        return;
    }

    QJsonObject message;
    message["type"] = "connect";
    QJsonDocument doc(message);
    if (tcpSocket && tcpSocket->state() == QAbstractSocket::ConnectedState) {
        tcpSocket->write(doc.toJson());
    }
    else
        emit lostConnect();
    return;
}
