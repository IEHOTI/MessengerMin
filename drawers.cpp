#include "mainwindow.h"
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QTcpServer>
#include <QTextEdit>
#include <QProgressBar>
#include <QDir>
#include <QTextBrowser>

void MainWindow::drawInterface() {
    mainLayout = new QGridLayout(mainWidget);
    QWidget *generalWidget = new QWidget(mainWidget);
    drawPeerPanel(generalWidget);
    mainLayout->addWidget(generalWidget,0,0);
    QWidget *userWidget = new QWidget(mainWidget);
    drawUserPanel(userWidget);
    mainLayout->addWidget(userWidget,1,0);

    connect(tcpServer, &QTcpServer::newConnection,
            this, &MainWindow::onNewConnection);
    //connect na redraw generalWidget
    connect(this, &MainWindow::successConnect,
            this, [=]() {
                drawDialoguePanel(generalWidget);
                showNotification(NotificationType::Information, QString("Соединение с %1 установлено успешно").arg(connectedPeerName));
                state = Connected;
                emit successConnectToButton();
            });

    connect(this, &MainWindow::lostConnect,
            this, [=]() {
                state = Disconnected;
                drawPeerPanel(generalWidget);
                showNotification(NotificationType::Information, QString("Соединение с %1 разорвано").arg(connectedPeerName));
                connectedPeerName.clear();
                connectedPeerIP.clear();
                emit lostConnectToButton();
            });
}

void MainWindow::drawUserPanel(QWidget *widget) {
    widget->setFixedHeight(50);
    QGridLayout *layout = new QGridLayout(widget);

    QLabel *name = new QLabel("Имя устройства", widget);
    name->setFixedHeight(40);
    layout->addWidget(name, 0, 0);

    QLineEdit *line = new QLineEdit(this->localName, widget);
    line->setFixedHeight(40);
    line->setReadOnly(true);
    layout->addWidget(line, 0, 1);

    QPushButton *buttonEdit = new QPushButton(widget);
    buttonEdit->setIcon(QIcon(":/page/edit.png"));
    buttonEdit->setFixedSize(40, 40);
    layout->addWidget(buttonEdit, 0, 2);

    QPushButton *buttonConfirm = new QPushButton(widget);
    buttonConfirm->setIcon(QIcon(":/page/done.png"));
    buttonConfirm->setFixedSize(40, 40);
    buttonConfirm->setVisible(false);
    layout->addWidget(buttonConfirm, 0, 2);

    QPushButton *buttonCancel = new QPushButton(widget);
    buttonCancel->setIcon(QIcon(":/page/cancel.png"));
    buttonCancel->setFixedSize(40, 40);
    buttonCancel->setVisible(false);
    layout->addWidget(buttonCancel, 0, 3);

    connect(buttonEdit, &QPushButton::clicked, this, [=]() {
        buttonEdit->setVisible(false);
        buttonConfirm->setVisible(true);
        buttonCancel->setVisible(true);
        line->setReadOnly(false);
        line->setFocus();
    });

    connect(buttonConfirm, &QPushButton::clicked, this, [=]() {
        buttonConfirm->setVisible(false);
        buttonCancel->setVisible(false);
        buttonEdit->setVisible(true);
        line->setReadOnly(true);
        localName = line->text();
        broadcastDiscovery();
    });

    connect(buttonCancel, &QPushButton::clicked, this, [=]() {
        buttonConfirm->setVisible(false);
        buttonCancel->setVisible(false);
        buttonEdit->setVisible(true);
        line->setReadOnly(true);
        line->setText(localName);
    });

    QPushButton *buttonConnect = new QPushButton("Подключиться",widget);
    buttonConnect->setMaximumWidth(150);
    buttonConnect->setFixedHeight(40);
    layout->addWidget(buttonConnect,0,4);

    connect(buttonConnect,&QPushButton::clicked,//???
            this,[=](){
                if (state == Disconnected) {
                    buttonConnect->setText("Отключиться");
                    emit onPeerSelected();
                } else {
                    buttonConnect->setText("Подключиться");
                    onDisconnected();
                }
            });
    connect(this, &MainWindow::successConnectToButton,
            this, [=](){
                buttonConnect->setText("Отключиться");
            });
    connect(this, &MainWindow::lostConnectToButton,
            this, [=](){
                buttonConnect->setText("Подключиться");
            });
}

void MainWindow::drawPeerPanel(QWidget *widget) {
    disconnect(this, &MainWindow::updatePeerList, nullptr, nullptr);
    clearWidget(widget);

    QVBoxLayout *layout = new QVBoxLayout(widget);
    QListWidget *listPeers = new QListWidget(widget);
    layout->addWidget(listPeers);

    connect(this,&MainWindow::updatePeerList,this,[=](){
        removeInactivePeer();
        listPeers->clear();
        for(const auto &peer : discoveredPeers)
            listPeers->addItem("Устройство [ " + peer.name + " ] IP: " + peer.ip
                               + " Время обнаружения: " + peer.lastDiscover.time().toString());
        widget->update();
    });

    emit updatePeerList();

    connect(listPeers, &QListWidget::itemDoubleClicked, this, [=](QListWidgetItem *item){
        int index = listPeers->row(item);
        if (index >= 0 && index < discoveredPeers.size())
            initiateConnection(discoveredPeers[index].ip, discoveredPeers[index].tcpPort);
    });

    connect(this,&MainWindow::onPeerSelected,this,[=](){
        if(listPeers->count() < 1 || discoveredPeers.size() < 1) {
            showNotification(NotificationType::Error, "Не удалось подключиться, устройства не найдены.");
            emit lostConnectToButton();
            return;
        }
        int index = listPeers->currentRow();
        if (index >= 0 && index < discoveredPeers.size())
            initiateConnection(discoveredPeers[index].ip, discoveredPeers[index].tcpPort);
        else
            initiateConnection(discoveredPeers[0].ip, discoveredPeers[0].tcpPort);
    });
}

void MainWindow::drawDialoguePanel(QWidget *widget) {
    disconnect(this, &MainWindow::addMessageToHistory, nullptr, nullptr);
    clearWidget(widget);

    QVBoxLayout *layout = new QVBoxLayout(widget);

    // Панель прогресса передачи файла (скрыта по умолчанию)
    QWidget *transferPanel = new QWidget(widget);
    QHBoxLayout *transferLayout = new QHBoxLayout(transferPanel);

    fileTransferLabel = new QLabel("", transferPanel);
    fileProgressBar = new QProgressBar(transferPanel);
    fileProgressBar->setVisible(false);
    fileProgressBar->setMaximum(100);

    QPushButton *buttonCancel = new QPushButton("✕", transferPanel);
    buttonCancel->setFixedSize(25, 25);
    buttonCancel->setVisible(false);

    transferLayout->addWidget(fileTransferLabel);
    transferLayout->addWidget(fileProgressBar);
    transferLayout->addWidget(buttonCancel);
    transferPanel->setVisible(false);

    layout->addWidget(transferPanel);

    //QTextEdit *chatHistory = new QTextEdit(widget);
    QTextBrowser *chatHistory = new QTextBrowser(widget);
    chatHistory->setOpenExternalLinks(true);
    chatHistory->setReadOnly(true);
    layout->addWidget(chatHistory);

    QHBoxLayout *inputLayout = new QHBoxLayout(widget);

    QLineEdit *messageInput = new QLineEdit(widget);
    messageInput->setPlaceholderText("Введите сообщение...");
    inputLayout->addWidget(messageInput);

    QPushButton *buttonSend = new QPushButton(widget);
    buttonSend->setIcon(QIcon(QPixmap(":/page/send.png")));
    inputLayout->addWidget(buttonSend);

    // Кнопка отправки файла
    buttonFile = new QPushButton(widget);
    buttonFile->setIcon(QIcon(QPixmap(":/page/file.png")));
    buttonFile->setToolTip("Отправить файл");
    buttonFile->setFixedSize(40, 40);
    inputLayout->addWidget(buttonFile);

    // Подключения для отправки файлов
    connect(buttonFile, &QPushButton::clicked, this, &MainWindow::onSendFileClicked);
    connect(buttonCancel, &QPushButton::clicked, this, &MainWindow::cancelFileTransfer);

    layout->addLayout(inputLayout);

    connect(buttonSend, &QPushButton::clicked,
            this, [=](){
                QString text = messageInput->text().trimmed();
                if (!text.isEmpty()) {
                    sendMessage(text);
                    messageInput->clear();
                }
            });
    connect(messageInput, &QLineEdit::returnPressed,
            buttonSend, &QPushButton::click);

    connect(this, &MainWindow::addMessageToHistory,
            this, [=](const QString &sender, const QString &text,
                bool isOutgoing, bool isFile = false,
                const QString &fileName = "", qint64 fileSize = 0) {
                QString formattedMessage;
                QDateTime currentTime = QDateTime::currentDateTime();

                if (isFile) {
                    // Форматирование для файла
                    QString sizeStr;
                    if (fileSize < 1024) sizeStr = QString("%1 Б").arg(fileSize);
                    else if (fileSize < 1024*1024) sizeStr = QString("%1 КБ").arg(fileSize/1024.0, 0, 'f', 1);
                    else sizeStr = QString("%1 МБ").arg(fileSize/(1024.0*1024.0), 0, 'f', 1);

                    if (isOutgoing) {
                        formattedMessage = QString("[%1] 📎 Вы отправили файл: <b>%2</b> (%3)")
                                               .arg(currentTime.toString("HH:mm"))
                                               .arg(fileName).arg(sizeStr);
                        chatHistory->setTextColor(QColor(0, 100, 0)); // Темно-зеленый
                    } else {
                        formattedMessage = QString("[%1] 📎 %2 отправил(а) файл: <b>%3</b> (%4)")
                                               .arg(currentTime.toString("HH:mm"))
                                               .arg(sender).arg(fileName).arg(sizeStr);
                        chatHistory->setTextColor(QColor(0, 0, 150)); // Темно-синий
                    }

                    // Делаем имя файла кликабельным
                    formattedMessage.replace(
                        QString(">%2<").arg(fileName),
                        QString("><a href='file://%1/%2'>%2</a><")
                            .arg(QDir::currentPath()).arg(fileName)
                        );

                } else {
                    // Обычное текстовое сообщение
                    if (isOutgoing) {
                        formattedMessage = QString("[%1] Вы: %2")
                                               .arg(currentTime.toString("HH:mm")).arg(text);
                        chatHistory->setTextColor(Qt::darkGreen);
                    } else {
                        formattedMessage = QString("[%1] %2: %3")
                        .arg(currentTime.toString("HH:mm"))
                            .arg(sender).arg(text);
                        chatHistory->setTextColor(Qt::black);
                    }
                }

                // Добавляем HTML-форматирование
                chatHistory->append(formattedMessage);

                // Прокрутка вниз
                QTextCursor cursor = chatHistory->textCursor();
                cursor.movePosition(QTextCursor::End);
                chatHistory->setTextCursor(cursor);
            });
    //     QString formattedMessage;
    //     QDateTime currentTime = QDateTime::currentDateTime();
    //     if (isOutgoing) {
    //         formattedMessage = QString("[%1] Вы: %2")
    //                                .arg(currentTime.toString("HH:mm")).arg(text);
    //         chatHistory->setTextColor(Qt::green);
    //     } else {
    //         formattedMessage = QString("[%1] %2: %3")
    //                                 .arg(currentTime.toString("HH:mm"))
    //                                 .arg(sender).arg(text);
    //         chatHistory->setTextColor(Qt::black);
    //     }
    //     chatHistory->append(formattedMessage);

    //     QTextCursor cursor = chatHistory->textCursor();
    //     cursor.movePosition(QTextCursor::End);
    //     chatHistory->setTextCursor(cursor);
    // });
}

void MainWindow::clearWidget(QWidget *widget) {
    if(!widget) return;

    QLayout *oldLayout = widget->layout();
    if (oldLayout) {
        QLayoutItem *item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->hide();
                item->widget()->setEnabled(false);
                item->widget()->deleteLater();
                item->widget()->disconnect();
                disconnect(item->widget());
            }
            delete item;
        }
        delete oldLayout;
    }
}
