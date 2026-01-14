#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "models.h"

class QUdpSocket;
class QTcpSocket;
class QTcpServer;
class QGridLayout;
class QMessageBox;
class QLabel;
class QPushButton;
class QProgressBar;
class QFile;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setUpNetwork();//
    void initiateConnection(const QString &ip, quint16 tcpPort);//
    void sendMessage(const QString &text);//
    void sendConfirmConnection();

    void addPeer(const QString &name, const QString &ip, quint16 tcpPort);//
    void removeInactivePeer();//

    void drawInterface();//
    void drawUserPanel(QWidget *widget);//
    void drawPeerPanel(QWidget *widget);//
    void drawDialoguePanel(QWidget *widget);//

    void clearWidget(QWidget *widget);//
    void setCentralWindow(int width, int height);//
    void showNotification(NotificationType type, const QString &message, int *reply = nullptr);//

    // Методы
    void generateTransferId(QString &resultStr);//
    void validateFile(const QString &filePath, bool &result);//
    void updateFileProgress(const QString &transferId, int progress);//
    void showFileInChat(const QString &fileName, qint64 fileSize, bool isOutgoing);//подумать еще?
    void successSendFile();
    void successReceiveFile();

signals:
    void updatePeerList();
    void onPeerSelected();
    void successConnect();
    //void successAccept();
    //void successGetPart();
    //void successSend();
    //void successReceive();
    void successConnectToButton();
    void lostConnect();
    void lostConnectToButton();

    void addMessageToHistory(const QString &sender, const QString &text,
                             bool isOutgoing, bool isFile = false,
                             const QString &fileName = "", qint64 fileSize = 0);

public slots:
    void onReadyReadUDP();//
    void onReadyReadTCP();//
    void broadcastDiscovery();//

    void onNewConnection();//
    void onDisconnected();//

    void onSendFileClicked();//
    void onFileSelected(const QString &filePath);//

    void sendFile(const QString &filePath);//?
    void sendNextChunk(const QJsonObject &obj);//?

    void receiveFile(const QJsonObject &obj);
    void processNextChunk(const QJsonObject &obj);

    void cancelFileTransfer();

private:
    ConnectionState state;
    QString connectedPeerName;
    QString connectedPeerIP;

    QList<PeerInfo> discoveredPeers;
    QString localName;
    quint16 udpPort;
    quint16 tcpPort;

    QWidget *mainWidget;
    QGridLayout *mainLayout;
    QMessageBox *notificationBox;

    QUdpSocket *udpSocket;
    QTcpSocket *tcpSocket;
    QTcpServer *tcpServer;
    QFile *file;
    QString transferId;

    QPushButton *buttonFile;
    QProgressBar *fileProgressBar;
    QLabel *fileTransferLabel;
};
#endif // MAINWINDOW_H
