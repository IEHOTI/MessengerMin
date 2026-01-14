#include "mainwindow.h"
#include <QScreen>
#include <QRect>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTcpServer>
#include <QMessageBox>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkInterface>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    state(Disconnected),
    tcpSocket(nullptr), tcpServer(nullptr), udpSocket(nullptr),
    mainLayout(nullptr), notificationBox(nullptr),
    tcpPort(60000), udpPort(60001),
    connectedPeerName(""), connectedPeerIP("")
{
    setWindowTitle("Локальный мессенджер - Min");
    mainWidget = new QWidget(this);
    localName = QSysInfo::machineHostName();

    setCentralWidget(mainWidget);
    this->setGeometry(0,0,800,600);
    setCentralWindow(800,600);

    setUpNetwork();
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout,
            this, &MainWindow::broadcastDiscovery);
    timer->start(10000);  // Рассылаем каждые 10 секунд
    broadcastDiscovery();

    drawInterface();
}

MainWindow::~MainWindow() {
    //позже
}

void MainWindow::setCentralWindow(int width, int height){
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();

    int x = (screenGeometry.width() - width) / 2;
    int y = (screenGeometry.height() - height) / 2;

    this->move(x, y);
}

void MainWindow::setUpNetwork() {
    udpSocket = new QUdpSocket(this);

    // Привязываемся к порту для получения broadcast сообщений
    if (!udpSocket->bind(udpPort, QUdpSocket::ShareAddress)) {
        showNotification(NotificationType::Error, "Не удалось привязать UDP-сокет к порту.");
        return;
    }

    connect(udpSocket, &QUdpSocket::readyRead, this, &MainWindow::onReadyReadUDP);

    // Настраиваем TCP сервер для входящих соединений
    tcpServer = new QTcpServer(this);
    if (!tcpServer->listen(QHostAddress::Any, tcpPort)) {
        showNotification(NotificationType::Error, "Не удалось запустить TCP сервер: " + tcpServer->errorString());
        return;
    }
}

void MainWindow::broadcastDiscovery() {
    QJsonObject message;
    message["type"] = "discovery";
    message["name"] = localName;
    message["tcp_port"] = tcpPort;

    QJsonDocument doc(message);
    QByteArray data = doc.toJson();

    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

    for (const QNetworkInterface &interface : interfaces) {
        if (   !(interface.flags() & QNetworkInterface::IsUp)
            || !(interface.flags() & QNetworkInterface::IsRunning)
            || interface.flags() & QNetworkInterface::IsLoopBack)
            continue;

        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            QHostAddress broadcastAddress = entry.broadcast();

            if (!broadcastAddress.isNull() && entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                udpSocket->writeDatagram(data, broadcastAddress, udpPort);
        }
    }
    if(state == Disconnected) emit updatePeerList();
}
