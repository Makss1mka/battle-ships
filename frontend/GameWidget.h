#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QWebSocket>

class QLabel;
class QPushButton;
class QGridLayout;

class GameWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameWidget(QNetworkAccessManager *networkManager, QWidget *parent = nullptr);
    void joinGame(const QString &sessionId, const QString &playerName);
    void leaveGame();

signals:
    void backToSessions();

private slots:
    void onCellClicked(int row, int col);
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketMessageReceived(const QString &message);
    void onBackClicked();

private:
    void setupBoard(QGridLayout *layout, bool isEnemy);
    void updateBoards(const QJsonObject &gameState);
    void showGameResult(const QString &result);
    void disableBoards();

    QNetworkAccessManager *networkManager;
    QWebSocket *webSocket;
    QString currentSessionId;
    QString currentPlayerName;
    int currentPlayerNumber;
    bool isMyTurn;

    QLabel *statusLabel;
    QPushButton *backButton;
    QGridLayout *playerBoardLayout;
    QGridLayout *enemyBoardLayout;
    QList<QList<QPushButton*>> playerBoardButtons;
    QList<QList<QPushButton*>> enemyBoardButtons;
};

#endif // GAMEWIDGET_H
