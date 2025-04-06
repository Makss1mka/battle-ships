#include "./GameWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QTimer>
#include <QDebug>

GameWidget::GameWidget(QNetworkAccessManager *networkManager, QWidget *parent)
    : QWidget(parent), networkManager(networkManager), webSocket(nullptr), currentPlayerNumber(0), isMyTurn(false) {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    statusLabel = new QLabel("Подключение к игре...", this);
    mainLayout->addWidget(statusLabel);

    QHBoxLayout *boardsLayout = new QHBoxLayout();

    // Player board
    QWidget *playerBoard = new QWidget(this);
    playerBoardLayout = new QGridLayout(playerBoard);
    setupBoard(playerBoardLayout, false);
    boardsLayout->addWidget(playerBoard);

    // Enemy board
    QWidget *enemyBoard = new QWidget(this);
    enemyBoardLayout = new QGridLayout(enemyBoard);
    setupBoard(enemyBoardLayout, true);
    boardsLayout->addWidget(enemyBoard);

    mainLayout->addLayout(boardsLayout);

    backButton = new QPushButton("Вернуться к списку", this);
    connect(backButton, &QPushButton::clicked, this, &GameWidget::onBackClicked);
    mainLayout->addWidget(backButton);
}

void GameWidget::setupBoard(QGridLayout *layout, bool isEnemy) {
    QList<QList<QPushButton*>> &buttons = isEnemy ? enemyBoardButtons : playerBoardButtons;

    for (int row = 0; row < 10; ++row) {
        QList<QPushButton*> rowButtons;
        for (int col = 0; col < 10; ++col) {
            QPushButton *button = new QPushButton();
            button->setFixedSize(30, 30);
            button->setProperty("row", row);
            button->setProperty("col", col);

            if (isEnemy) {
                connect(button, &QPushButton::clicked, [this, row, col]() { onCellClicked(row, col); });
            }

            layout->addWidget(button, row, col);
            rowButtons.append(button);
        }
        buttons.append(rowButtons);
    }
}

void GameWidget::joinGame(const QString &sessionId, const QString &playerName) {
    leaveGame();

    currentSessionId = sessionId;
    currentPlayerName = playerName;

    if (webSocket) {
        webSocket->deleteLater();
    }

    webSocket = new QWebSocket();
    connect(webSocket, &QWebSocket::connected, this, &GameWidget::onWebSocketConnected);
    connect(webSocket, &QWebSocket::disconnected, this, &GameWidget::onWebSocketDisconnected);
    connect(webSocket, &QWebSocket::textMessageReceived, this, &GameWidget::onWebSocketMessageReceived);

    webSocket->open(QUrl("ws://localhost:9000"));
}

void GameWidget::leaveGame() {
    if (webSocket) {
        disconnect(webSocket, nullptr, this, nullptr);

        if (webSocket->state() == QAbstractSocket::ConnectedState) {
            QJsonObject message;
            message["type"] = "leave";
            message["session_id"] = currentSessionId;
            webSocket->sendTextMessage(QJsonDocument(message).toJson());

            webSocket->close(QWebSocketProtocol::CloseCodeNormal);
        }
        webSocket->deleteLater();
        webSocket = nullptr;
    }

    currentSessionId.clear();
    currentPlayerName.clear();
    currentPlayerNumber = 0;
    isMyTurn = false;
}

void GameWidget::onWebSocketConnected() {
    qDebug() << "WebSocket connected";

    QJsonObject message;
    message["type"] = "join";
    message["session_id"] = currentSessionId;
    message["player_name"] = currentPlayerName;
    webSocket->sendTextMessage(QJsonDocument(message).toJson());

    qDebug() << "Connected";
}

void GameWidget::onWebSocketDisconnected() {
    qDebug() << "WebSocket disconnected";
    showGameResult("Соединение с сервером потеряно");
}

void GameWidget::onWebSocketMessageReceived(const QString &message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject json = doc.object();

    QString type = json["type"].toString();

    if (type == "game_state") {
        currentPlayerNumber = json["your_player_number"].toInt();
        isMyTurn = (json["current_player"].toInt() == currentPlayerNumber);
        updateBoards(json);
        statusLabel->setText(isMyTurn ? "Ваш ход" : "Ход противника");
    } else if (type == "player_left") {
        showGameResult("Партия прервана, игрок вышел");
    } else if (type == "attack_result") {
        bool game_over = json["game_over"].toBool();
        if (game_over) {
            QString winner;
            if (currentPlayerNumber == json["next_player"].toInt()) {
                winner = "Вы выиграли!";
            } else {
                winner = QString("Игрок %1 выиграл! Вы проиграли :(").arg(currentPlayerName);
            }
            showGameResult(winner);
        }
    }
}

void GameWidget::updateBoards(const QJsonObject &gameState) {
    QJsonObject playerBoard = gameState["player_board"].toObject();
    QJsonArray cells = playerBoard["cells"].toArray();

    for (int row = 0; row < 10; ++row) {
        QJsonArray rowCells = cells[row].toArray();
        for (int col = 0; col < 10; ++col) {
            int cellState = rowCells[col].toInt();
            QPushButton *button = playerBoardButtons[row][col];

            QString style;
            switch (cellState) {
                case 0: style = "background-color: blue;"; break;       // EMPTY
                case 1: style = "background-color: gray;"; break;       // SHIP
                case 2: style = "background-color: red;"; break;        // HIT
                case 3: style = "background-color: white;"; break;      // MISS
            }
            button->setStyleSheet(style);
        }
    }

    QJsonObject enemyBoard = gameState["enemy_board"].toObject();
    cells = enemyBoard["cells"].toArray();

    for (int row = 0; row < 10; ++row) {
        QJsonArray rowCells = cells[row].toArray();
        for (int col = 0; col < 10; ++col) {
            int cellState = rowCells[col].toInt();
            QPushButton *button = enemyBoardButtons[row][col];

            QString style;
            switch (cellState) {
                case 2: style = "background-color: red;"; break;        // HIT
                case 3: style = "background-color: white;"; break;      // MISS
                default: style = "background-color: blue;"; break;      // EMPTY or SHIP (не показываем)
            }
            button->setStyleSheet(style);
        }
    }
}

void GameWidget::onCellClicked(int row, int col) {
    if (!isMyTurn || !webSocket || webSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonObject message;
    message["type"] = "attack";
    message["session_id"] = currentSessionId;
    message["x"] = col;
    message["y"] = row;

    webSocket->sendTextMessage(QJsonDocument(message).toJson());
    isMyTurn = false;
    statusLabel->setText("Ожидание хода противника...");
}

void GameWidget::showGameResult(const QString &result) {
    QMessageBox::information(this, "Игра окончена", result);
    //disableBoards();
    leaveGame();
    emit backToSessions();
}

void GameWidget::disableBoards() {
    for (auto &row : enemyBoardButtons) {
        for (auto button : row) {
            button->setEnabled(false);
        }
    }
}

void GameWidget::onBackClicked() {
    leaveGame();
    emit backToSessions();
}
