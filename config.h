#ifndef CONFIG_H
#define CONFIG_H

#include <QStringList>

namespace TransferConfig {
// Максимальный размер файла (120 МБ)
const qint64 MAX_FILE_SIZE = 120 * 1024 * 1024;

// Размер одной части файла (64 КБ)
const int CHUNK_SIZE = 64 * 1024;

// Таймаут ожидания частей (секунды)
const int TRANSFER_TIMEOUT = 30;

// Папка для загруженных файлов
const QString DOWNLOAD_DIR = "downloads";

// Поддерживаемые типы файлов
const QStringList SUPPORTED_EXTENSIONS = {
    ".txt", ".pdf", ".doc", ".docx", ".xls", ".xlsx",
    ".jpg", ".jpeg", ".png", ".gif", ".bmp",
    ".zip", ".rar", ".7z",
    ".mp3", ".wav",
    ".mp4", ".avi", ".mkv"
};

}

#endif // CONFIG_H
