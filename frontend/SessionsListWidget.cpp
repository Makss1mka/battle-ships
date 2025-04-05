#include "./SessionsListWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QDateTime>

SessionsListWidget::SessionsListWidget(QNetworkAccessManager *networkManager, QWidget *parent)
    : QWidget(parent), networkManager(networkManager) {
    QVBoxLayout *layout = new QVBoxLayout(this);

    playerNameEdit = new QLineEdit(this);
    playerNameEdit->setPlaceholderText("Введите ваше имя");
    layout->addWidget(playerNameEdit);

    QHBoxLayout *buttonsLayout = new QHBoxLayout();

    refreshButton = new QPushButton("Обновить список", this);
    connect(refreshButton, &QPushButton::clicked, this, &SessionsListWidget::onRefreshClicked);
    buttonsLayout->addWidget(refreshButton);

    createButton = new QPushButton("Создать сессию", this);
    connect(createButton, &QPushButton::clicked, this, &SessionsListWidget::onCreateClicked);
    buttonsLayout->addWidget(createButton);

    layout->addLayout(buttonsLayout);

    sessionsList = new QListWidget(this);
    sessionsList->setStyleSheet("QListWidget { font-size: 14px; }");
    connect(sessionsList, &QListWidget::doubleClicked, this, &SessionsListWidget::onSessionDoubleClicked);
    layout->addWidget(sessionsList);

    refreshSessions();
}

void SessionsListWidget::refreshSessions() {
    QNetworkRequest request(QUrl("http://localhost:8080/sessions"));
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &SessionsListWidget::onSessionsReceived);
}

void SessionsListWidget::onSessionDoubleClicked(const QModelIndex &index) {
    QString playerName = playerNameEdit->text().trimmed();
    if (playerName.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите ваше имя");
        return;
    }

    QListWidgetItem *item = sessionsList->item(index.row());
    QString sessionId = item->data(Qt::UserRole).toString();
    emit sessionSelected(sessionId, playerName);
}

void SessionsListWidget::onRefreshClicked() {
    refreshSessions();
}

void SessionsListWidget::onSessionsReceived() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        QJsonArray sessions = doc.array();

        sessionsList->clear();
        for (const QJsonValue &value : sessions) {
            QJsonObject session = value.toObject();
            QString id = session["id"].toString();
            QString player1 = session["player1"].toString();
            qint64 createdAt = session["created_at"].toInt();

            QDateTime dt;
            dt.setSecsSinceEpoch(createdAt);
            QString timeStr = dt.toString("dd.MM.yyyy HH:mm");

            QListWidgetItem *item = new QListWidgetItem(
                QString("Сессия: %1\nИгрок: %2\nСоздана: %3")
                    .arg(id.left(8) + "...")
                    .arg(player1)
                    .arg(timeStr)
                );
            item->setData(Qt::UserRole, id);
            sessionsList->addItem(item);
        }
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось получить список сессий");
    }

    reply->deleteLater();
}

void SessionsListWidget::onCreateClicked() {
    QString playerName = playerNameEdit->text().trimmed();
    if (playerName.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите ваше имя");
        return;
    }
    emit createSessionRequested(playerName);
}
