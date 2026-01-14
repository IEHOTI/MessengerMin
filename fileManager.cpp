#include "mainwindow.h"
#include "config.h"
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QTcpSocket>
#include <QRandomGenerator>
#include <QThread>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QCoreApplication>
#include <QStandardPaths>

void MainWindow::onSendFileClicked() {
    if (!tcpSocket || !tcpSocket->isOpen()) {
        showNotification(NotificationType::Error, "Нет активного соединения");
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Выберите файл для отправки",
        QDir::homePath(),
        "Все файлы (*.*)"
        );

    if (!filePath.isEmpty()) {
        onFileSelected(filePath);
    }
}

void MainWindow::validateFile(const QString &filePath, bool &result) {
    QFileInfo fileInfo(filePath);
    result = true;

    if (!fileInfo.exists()) {
        showNotification(NotificationType::Error, "Файл не существует");
        result = false;
        return;
    }

    if (fileInfo.size() > TransferConfig::MAX_FILE_SIZE) {
        showNotification(NotificationType::Error,
                         QString("Файл слишком большой (максимум %1 МБ)")
                             .arg(TransferConfig::MAX_FILE_SIZE / (1024 * 1024)));
        result = false;
        return;
    }

    if (!fileInfo.isReadable()) {
        showNotification(NotificationType::Error, "Нет прав на чтение файла");
        result = false;
        return;
    }

    return;
}

void MainWindow::onFileSelected(const QString &filePath) {
    bool result = false;
    validateFile(filePath, result);
    if (!result) return;

    QFileInfo fileInfo(filePath);
    qint64 fileSize = fileInfo.size();

    // Показываем диалог подтверждения
    QString sizeStr;
    if (fileSize < 1024) sizeStr = QString("%1 Б").arg(fileSize);
    else if (fileSize < 1024*1024) sizeStr = QString("%1 КБ").arg(fileSize/1024.0, 0, 'f', 1);
    else sizeStr = QString("%1 МБ").arg(fileSize/(1024.0*1024.0), 0, 'f', 1);

    QString message = QString("Отправить файл?\n\n"
                              "Имя: %1\n"
                              "Размер: %2\n"
                              "Тип: %3")
                          .arg(fileInfo.fileName())
                          .arg(sizeStr)
                          .arg(fileInfo.suffix().toUpper());

    int reply;
    showNotification(NotificationType::Question, message, &reply);

    if (reply == QMessageBox::Yes) {
        showNotification(NotificationType::Information,
                         QString("Ожидайте подтверждения получения от %1").arg(connectedPeerName));
        sendFile(filePath);
    }
}

void MainWindow::generateTransferId(QString &result) {
    result = QString("%1_%2")
    .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->bounded(1000, 99999));
    return;
}

void MainWindow::sendFile(const QString &filePath) {
    file = new QFile(filePath);
    if(!file->open(QIODevice::ReadOnly)) {
        showNotification(NotificationType::Error, "Не удалось открыть файл");
        file->close();
        file->deleteLater();
        return;
    }
    if (!file->seek(0)) {
        showNotification(NotificationType::Error, "Ошибка позиционирования в файле");
        file->close();
        file->deleteLater();
        return;
    }

    QFileInfo fileInfo(filePath);
    generateTransferId(transferId);
    qint64 fileSize = fileInfo.size();

    // Отправляем метаданные файла
    QJsonObject fileInfoMsg;
    fileInfoMsg["type"] = "file_info";
    fileInfoMsg["transfer_id"] = transferId;
    fileInfoMsg["file_name"] = fileInfo.fileName();
    fileInfoMsg["file_size"] = QString::number(fileSize);

    QJsonDocument doc(fileInfoMsg);
    if (tcpSocket && tcpSocket->state() == QAbstractSocket::ConnectedState) {
        tcpSocket->write(doc.toJson());
        tcpSocket->flush();
    } else {
        file->close();
        file->deleteLater();
        transferId.clear();
        emit lostConnect();
        return;
    }

    // Показываем панель прогресса
    if (fileTransferLabel && fileProgressBar) {
        fileTransferLabel->setText(QString("Отправка: %1").arg(fileInfo.fileName()));
        fileProgressBar->setVisible(true);
        fileProgressBar->setValue(0);

        QWidget *transferPanel = fileTransferLabel->parentWidget();
        if (transferPanel) {
            transferPanel->setVisible(true);
            QPushButton *cancelBtn = transferPanel->findChild<QPushButton*>();
            if (cancelBtn) cancelBtn->setVisible(true);
        }
    }
}

void MainWindow::sendNextChunk(const QJsonObject &obj) {
    if(file->atEnd()) {
        qDebug() << "end file";
        // Отправляем метаданные файла
        QJsonObject fileInfoMsg;
        fileInfoMsg["type"] = "file_done";

        QJsonDocument doc(fileInfoMsg);
        if (tcpSocket && tcpSocket->state() == QAbstractSocket::ConnectedState) {
            tcpSocket->write(doc.toJson());
            tcpSocket->flush();
            successSendFile();
            return;
        } else {
            emit lostConnect();
            return;
        }
    }
    quint64 tempSize = file->size();
    int sizeChunk = 1024; //experimental
    int tempChunk = obj["chunk"].toInt();
    double tempProgress = tempChunk * sizeChunk * 100 / tempSize;
    fileProgressBar->setValue(tempProgress);
    //сюда добавить рассчет какой-то прогрессии
    QByteArray chunk = file->read(sizeChunk); // TransferConfig::CHUNK_SIZE
    QJsonObject fileData;
    fileData["type"] = "file_data";
    fileData["transfer_id"] = transferId;
    fileData["chunk"] = tempChunk;
    fileData["progress"] = QString::number(tempProgress > 100.0 ? 100.0 : tempProgress, 'f', 2);
    fileData["data"] = QString(chunk.toBase64());

    QJsonDocument doc(fileData);
    tcpSocket->write(doc.toJson());
    tcpSocket->flush();
}

void MainWindow::successSendFile() {
    emit addMessageToHistory(localName, "", true, true,
                             file->fileName(), file->size());
    file->close();
    file->deleteLater();
    transferId.clear();
    if (fileProgressBar) {
        fileProgressBar->setVisible(false);
        fileProgressBar->setValue(0);
    }
    if (fileTransferLabel) {
        fileTransferLabel->setText("");
        QWidget *transferPanel = fileTransferLabel->parentWidget();
        if (transferPanel) transferPanel->setVisible(false);
    }
    showNotification(NotificationType::Information,
                     "Успешная отправка файла");
}

void MainWindow::receiveFile(const QJsonObject &obj) {
    transferId = obj["transfer_id"].toString();
    QString fileName = obj["file_name"].toString();
    qint64 fileSize = obj["file_size"].toString().toLongLong();

    // Проверяем, можно ли принять файл
    if (fileSize > TransferConfig::MAX_FILE_SIZE) {
        // Отправляем отказ
        QJsonObject errorMsg;
        errorMsg["type"] = "file_error";
        errorMsg["transfer_id"] = transferId;
        errorMsg["reason"] = "file_too_large";

        QJsonDocument doc(errorMsg);
        tcpSocket->write(doc.toJson());
        return;
    }

    // Запрашиваем подтверждение у пользователя
    QString sizeStr;
    if (fileSize < 1024) sizeStr = QString("%1 Б").arg(fileSize);
    else if (fileSize < 1024*1024) sizeStr = QString("%1 КБ").arg(fileSize/1024.0, 0, 'f', 1);
    else sizeStr = QString("%1 МБ").arg(fileSize/(1024.0*1024.0), 0, 'f', 1);

    QString message = QString("%1 хочет отправить вам файл:\n\n"
                              "Имя: %2\n"
                              "Размер: %3\n"
                              "Принять файл?")
                          .arg(connectedPeerName)
                          .arg(fileName)
                          .arg(sizeStr);
    int reply;
    showNotification(NotificationType::Question, message, &reply);
    if (reply == QMessageBox::Yes) {
        QString dirPath = QFileDialog::getSaveFileName(
            this,
            "Куда сохранить файл?",
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + fileName,
            "Все файлы (*.*)"
            );

        if (dirPath.isEmpty()) {
            // Отказ пользователя
            QJsonObject rejectMsg;
            rejectMsg["type"] = "file_error";
            rejectMsg["transfer_id"] = transferId;
            rejectMsg["reason"] = "cancelled_by_user";
            tcpSocket->write(QJsonDocument(rejectMsg).toJson());
            return;
        }

        // Проверяем директорию
        QFileInfo fileInfo(dirPath);
        QString directory = fileInfo.absolutePath();
        QDir dir(directory);

        // Создаем директорию, если не существует
        if (!dir.exists()) {
            if (!dir.mkpath(directory)) {
                showNotification(NotificationType::Error,
                                 QString("Не удалось создать папку:\n%1").arg(directory));

                QJsonObject rejectMsg;
                rejectMsg["type"] = "file_error";
                rejectMsg["transfer_id"] = transferId;
                rejectMsg["reason"] = "cannot_create_directory";
                tcpSocket->write(QJsonDocument(rejectMsg).toJson());
                return;
            }
        }

        if (!QFileInfo(directory).isWritable()) {
            showNotification(NotificationType::Error,
                             QString("Нет прав на запись в папку:\n%1").arg(directory));

            QJsonObject rejectMsg;
            rejectMsg["type"] = "file_error";
            rejectMsg["transfer_id"] = transferId;
            rejectMsg["reason"] = "directory_not_writable";
            tcpSocket->write(QJsonDocument(rejectMsg).toJson());
            return;
        }

        if (QFile::exists(dirPath)) {
            int overwrite;
            showNotification(NotificationType::Question,
                             QString("Файл '%1' уже существует. Перезаписать?")
                             .arg(fileInfo.fileName()),
                             &overwrite);

            if (overwrite == QMessageBox::No) {
                QJsonObject rejectMsg;
                rejectMsg["type"] = "file_error";
                rejectMsg["transfer_id"] = transferId;
                rejectMsg["reason"] = "file_exists";
                tcpSocket->write(QJsonDocument(rejectMsg).toJson());
                return;
            }
        }

        file = new QFile(dirPath);
        if(!file->open(QIODevice::WriteOnly)) {
            showNotification(NotificationType::Error,
                             "Не удалось скачать файл: выбранная директория недоступна.");
            QJsonObject rejectMsg;
            rejectMsg["type"] = "file_error";
            rejectMsg["transfer_id"] = transferId;
            rejectMsg["reason"] = "uncorrect_path_by_user";

            QJsonDocument doc(rejectMsg);
            tcpSocket->write(doc.toJson());
            return;
        }

        QJsonObject message;
        message["type"] = "file_next";
        message["transfer_id"] = transferId;
        message["chunk"] = QString::number(0);
        QJsonDocument docx(message);
        if (tcpSocket && tcpSocket->state() == QAbstractSocket::ConnectedState) {
            tcpSocket->write(docx.toJson());
            tcpSocket->flush();
        }
        else
        {
            emit lostConnect();
        }

        // Показываем панель прогресса
        if (fileTransferLabel && fileProgressBar) {
            fileTransferLabel->setText(QString("Прием: %1").arg(fileName));
            fileProgressBar->setVisible(true);
            fileProgressBar->setValue(0);

            QWidget *transferPanel = fileTransferLabel->parentWidget();
            if (transferPanel) {
                transferPanel->setVisible(true);
                QPushButton *cancelBtn = transferPanel->findChild<QPushButton*>();
                if (cancelBtn) cancelBtn->setVisible(true);
            }
        }
    } else {
        // Отправляем отказ
        QJsonObject rejectMsg;
        rejectMsg["type"] = "file_error";
        rejectMsg["transfer_id"] = transferId;
        rejectMsg["reason"] = "rejected_by_user";

        QJsonDocument doc(rejectMsg);
        tcpSocket->write(doc.toJson());
    }
}

void MainWindow::processNextChunk(const QJsonObject &obj) {
    if(transferId != obj["transfer_id"].toString()) qDebug() << "mismatch id";//later
    int progress = obj["progress"].toInt();
    QByteArray data = QByteArray::fromBase64(obj["data"].toString().toUtf8());
    file->write(data);
    file->flush();
    fileProgressBar->setValue(progress);
    QJsonObject message;
    message["type"] = "file_next";
    message["transfer_id"] = transferId;
    message["chunk"] = obj["chunk"].toInt() + 1;

    QJsonDocument docx(message);
    if (tcpSocket && tcpSocket->state() == QAbstractSocket::ConnectedState) {
        tcpSocket->write(docx.toJson());
        tcpSocket->flush();
        tcpSocket->waitForBytesWritten();
    }
    else
    {
        emit lostConnect();
    }
}

void MainWindow::successReceiveFile() {
    if(!file->isOpen()) {
        qDebug() << "Файл не открыт, что?...";
        return;
    }
    file->close();
    file->deleteLater();
    transferId.clear();
    if (fileProgressBar) {
        fileProgressBar->setVisible(false);
        fileProgressBar->setValue(0);
    }
    if (fileTransferLabel) {
        fileTransferLabel->setText("");
        QWidget *transferPanel = fileTransferLabel->parentWidget();
        if (transferPanel) transferPanel->setVisible(false);
    }
    showNotification(NotificationType::Information,
                     "Файл получен успешно");
}


void MainWindow::cancelFileTransfer() {
    // TODO: Реализовать отмену передачи
    showNotification(NotificationType::Information, "Отмена передачи файла...");

    // Скрываем панель прогресса
    if (fileProgressBar) fileProgressBar->setVisible(false);
    if (fileTransferLabel) {
        fileTransferLabel->setText("");
        QWidget *transferPanel = fileTransferLabel->parentWidget();
        if (transferPanel) transferPanel->setVisible(false);
    }
}
