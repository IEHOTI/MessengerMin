#include "mainwindow.h"
#include <QMessageBox>

void MainWindow::showNotification(NotificationType type, const QString &message, int *reply) {
    if (notificationBox && notificationBox->isVisible()) {
        notificationBox->close();
        notificationBox = nullptr;
    }

    QMessageBox* msgBox = new QMessageBox(this);
    notificationBox = msgBox;  // Сохраняем указатель

    switch (type) {
    case NotificationType::Error:{
        msgBox->setIcon(QMessageBox::Critical);
        msgBox->setWindowTitle("Ошибка");
        break;
    }
    case NotificationType::Information: {
        msgBox->setIcon(QMessageBox::Information);
        msgBox->setWindowTitle("Информация");
        break;
    }
    case NotificationType::Warning: {
        msgBox->setIcon(QMessageBox::Warning);
        msgBox->setWindowTitle("Предупреждение");
        break;
    }
    case NotificationType::Question: {
        msgBox->setIcon(QMessageBox::Question);
        msgBox->setWindowTitle("Вопрос");
        msgBox->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        break;
    }
    }

    msgBox->setText(message);
    msgBox->setModal(false);  // Неблокирующее окно!

    // Автоматически удаляем при закрытии
    msgBox->setAttribute(Qt::WA_DeleteOnClose);

    // Соединяем сигнал закрытия для очистки указателя
    connect(msgBox, &QMessageBox::finished, this, [this, msgBox]() {
        if (notificationBox == msgBox)
            notificationBox = nullptr;
    });
    //фикс:
    if (type == NotificationType::Question && reply) {
        // Для вопроса нужен блокирующий вызов
        msgBox->setModal(true);
        if(reply) *reply = msgBox->exec();
        notificationBox = nullptr;
        //msgBox->deleteLater();
    } else {
        msgBox->show();
    }
}
