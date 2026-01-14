#include "mainwindow.h"
#include <QTcpSocket>
#include <QTcpServer>
#include <QTimer>
#include <qmessagebox.h>

void MainWindow::onNewConnection() {
    QTcpSocket *clientSocket = tcpServer->nextPendingConnection();
    int reply;
    QHostAddress addr = clientSocket->peerAddress();
    QString ipString;
    ipString = addr.toString();
    if (ipString.startsWith("::ffff:")) ipString = ipString.mid(7);
    QString message = QString("Устройство %1 хочет подключиться. Принять?").arg(ipString);
    showNotification(NotificationType::Question, message, &reply);

    if (reply == QMessageBox::Yes) {
        if (tcpSocket) {
            tcpSocket->disconnectFromHost();
            disconnect(tcpSocket);
            tcpSocket->disconnect();
            delete tcpSocket;
        }
        tcpSocket = clientSocket;
        ///
        connect(tcpSocket, &QTcpSocket::errorOccurred,
                this, [this](QAbstractSocket::SocketError error) {
                    qDebug() << "On new Connection Socket error:" << error << tcpSocket->errorString();
                    //onDisconnected();
                });
        ////
        connect(tcpSocket, &QTcpSocket::readyRead,
                this, &MainWindow::onReadyReadTCP);
        connect(tcpSocket, &QTcpSocket::disconnected,
                this, &MainWindow::onDisconnected);
        emit successConnect();
        sendConfirmConnection();
    } else {
        clientSocket->disconnectFromHost();
        delete clientSocket;
    }
}

void MainWindow::onDisconnected() {
    if (tcpSocket && state != Disconnected) {
        state = Disconnected;
        if (tcpSocket->state() == QAbstractSocket::ConnectedState)
            tcpSocket->disconnectFromHost();

        if (tcpSocket->state() == QAbstractSocket::UnconnectedState)
            tcpSocket->abort();

        tcpSocket->disconnect();
        disconnect(tcpSocket);

        QTcpSocket* oldSocket = tcpSocket;
        tcpSocket = nullptr;
        oldSocket->deleteLater();
        emit lostConnect();
    }
}

void MainWindow::initiateConnection(const QString &ip, quint16 tcpPort) {
    if (tcpSocket && tcpSocket->state() != QAbstractSocket::UnconnectedState) {
        tcpSocket->abort();
        QTcpSocket* oldSocket = tcpSocket;
        tcpSocket = nullptr;
        oldSocket->deleteLater();
    }

    if (state == Connected) { // поможет отловить момент когда сокет не очистился правильно, потом удалить
        int reply;
        showNotification(NotificationType::Question,
                         "У вас уже есть активное соединение. Отключиться и подключиться к новому устройству?",
                         &reply);
        if (reply == QMessageBox::No) return;
    }

    state = Connecting;
    tcpSocket = new QTcpSocket(this);
    /////
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        qDebug() << "Initiate Connection Socket error:" << error << tcpSocket->errorString();
        onDisconnected();
    });
    /////

    tcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    tcpSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    tcpSocket->connectToHost(ip, tcpPort);

    connect(tcpSocket, &QTcpSocket::connected, this, [=]() {
        for (const auto &peer : discoveredPeers) {
            if (peer.ip == ip && peer.tcpPort == tcpPort) {
                connectedPeerName = peer.name;
                break;
            }
        }
        connectedPeerIP = ip;
        showNotification(NotificationType::Information, QString("Ожидание подключения к %1").arg(connectedPeerName));

        connect(tcpSocket, &QTcpSocket::readyRead,
                this, &MainWindow::onReadyReadTCP);
        connect(tcpSocket, &QTcpSocket::disconnected,
                this,&MainWindow::onDisconnected);
    });
}
