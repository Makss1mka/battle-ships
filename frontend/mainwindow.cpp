#include "./mainwindow.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QInputDialog>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    networkManager(new QNetworkAccessManager(this)),
    stackedWidget(new QStackedWidget(this)),
    sessionsListWidget(new SessionsListWidget(networkManager, this)),
    gameWidget(new GameWidget(networkManager, this))
{
    setWindowTitle("Battleship");
    resize(800, 600);

    stackedWidget->addWidget(sessionsListWidget);
    stackedWidget->addWidget(gameWidget);
    setCentralWidget(stackedWidget);

    connect(sessionsListWidget, &SessionsListWidget::createSessionRequested, this, &MainWindow::handleCreateSession);
    connect(sessionsListWidget, &SessionsListWidget::sessionSelected, this, &MainWindow::showGameWidget);
    connect(gameWidget, &GameWidget::backToSessions, this, [this]() {
        stackedWidget->setCurrentIndex(0);
    });
}

MainWindow::~MainWindow() {
}

void MainWindow::showGameWidget(const QString &sessionId, const QString &playerName) {
    gameWidget->joinGame(sessionId, playerName);
    stackedWidget->setCurrentIndex(1);
}

void MainWindow::handleCreateSession(const QString &playerName) {
    QJsonObject json;
    json["player_name"] = playerName;

    QNetworkRequest request(QUrl("http://localhost:8080/create"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = networkManager->post(request, QJsonDocument(json).toJson());

    connect(reply, &QNetworkReply::finished, [this, reply, playerName]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(response);
            QJsonObject json = doc.object();

            QString sessionId = json["session_id"].toString();
            showGameWidget(sessionId, playerName);
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось создать сессию: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

void MainWindow::handleJoinSession(const QString &playerName) {
    QJsonObject json;
    json["player_name"] = playerName;

    QNetworkRequest request(QUrl("http://localhost:8080/join"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = networkManager->post(request, QJsonDocument(json).toJson());

    connect(reply, &QNetworkReply::finished, [this, reply, playerName]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(response);
            QJsonObject json = doc.object();

            QString sessionId = json["session_id"].toString();
            showGameWidget(sessionId, playerName);
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось подключиться к сессии: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

