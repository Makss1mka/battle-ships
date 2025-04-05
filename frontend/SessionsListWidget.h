#ifndef SESSIONSLISTWIDGET_H
#define SESSIONSLISTWIDGET_H

#include <QWidget>
#include <QNetworkAccessManager>

class QLineEdit;
class QPushButton;
class QListWidget;

class SessionsListWidget : public QWidget {
    Q_OBJECT

public:
    explicit SessionsListWidget(QNetworkAccessManager *networkManager, QWidget *parent = nullptr);
    void refreshSessions();

signals:
    void sessionSelected(const QString &sessionId, const QString &playerName);
    void createSessionRequested(const QString &playerName);

private slots:
    void onSessionDoubleClicked(const QModelIndex &index);
    void onRefreshClicked();
    void onCreateClicked();
    void onSessionsReceived();

private:
    QNetworkAccessManager *networkManager;
    QLineEdit *playerNameEdit;
    QPushButton *refreshButton;
    QPushButton *createButton;
    QListWidget *sessionsList;
};

#endif // SESSIONSLISTWIDGET_H
