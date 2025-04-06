// mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>  // Добавляем этот include
#include <QNetworkAccessManager>
#include "./SessionsListWidget.h"
#include "./GameWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void showGameWidget(const QString &sessionId, const QString &playerName);
    void handleCreateSession(const QString &playerName);
    void handleJoinSession(const QString &sessionId, const QString &playerName);

private:
    QNetworkAccessManager *networkManager;
    QStackedWidget *stackedWidget;
    SessionsListWidget *sessionsListWidget;
    GameWidget *gameWidget;
};

#endif // MAINWINDOW_H
