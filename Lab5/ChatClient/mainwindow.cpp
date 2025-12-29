#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHostAddress>
#include <QJsonValue>
#include <QJsonObject>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->stackedWidget->setCurrentWidget(ui->loginPage);
    m_chatclient = new ChatClient(this);
    connect(m_chatclient,&ChatClient::connected,this,&MainWindow::connectedToServer);
    connect(m_chatclient,&ChatClient::jsonReceived,this,&MainWindow::jsonReceived);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_loginButton_clicked()
{
    if(ui->userName->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "登录失败", "用户名不能为空");
        return;
    }
    m_chatclient->connectToServer(QHostAddress(ui->serverEdit->text()),1967);
}


void MainWindow::on_sayButton_clicked()
{
    if(!ui->sayLineEdit->text().isEmpty())
        m_chatclient->sendMessage(ui->sayLineEdit->text());
        ui->sayLineEdit->clear(); // 清空输入框
}


void MainWindow::on_logoutButton_clicked()
{
    m_chatclient->disconnectFromHost();
    ui->stackedWidget->setCurrentWidget(ui->loginPage);
    for(auto aItem : ui->userListWidget ->findItems(ui->userName->text(),Qt::MatchExactly)){
        qDebug("remove");
        ui->userListWidget->removeItemWidget(aItem);
        delete aItem;
    }
}

void MainWindow::connectedToServer()
{
    m_chatclient->sendMessage(ui->userName->text(),"login");
}

void MainWindow::messageReceived(const QString &sender, const QString &text)
{
    ui->roomTexitEdit->append(QString("%1 : %2").arg(sender).arg(text));
}

void MainWindow::jsonReceived(const QJsonObject &docObj)
{
    const QJsonValue typeVal = docObj.value("type");
    if(typeVal.isNull() || !typeVal.isString())
        return;

    // 添加处理登录错误
    if(typeVal.toString().compare("loginError",Qt::CaseInsensitive) == 0){
        const QJsonValue textVal = docObj.value("text");
        if(textVal.isNull() || !textVal.isString())
            return;

        const QString errorMsg = textVal.toString();
        // 在客户端控制台输出错误信息
        qDebug() << "登录失败：" << errorMsg;

        // 显示错误消息给用户
        QMessageBox::warning(this, "登录失败", errorMsg);

        // 断开连接，让用户重新输入
        m_chatclient->disconnectFromHost();
        ui->stackedWidget->setCurrentWidget(ui->loginPage);
        return;
    }

    if(typeVal.toString().compare("message",Qt::CaseInsensitive) == 0){
        const QJsonValue textVal = docObj.value("text");
        const QJsonValue senderVal = docObj.value("sender");
        if(textVal.isNull() || !textVal.isString())
            return;

        if(senderVal.isNull() || !senderVal.isString())
            return;

        const QString text = textVal.toString().trimmed();
        if(text.isEmpty())
            return;
        const QString sender = senderVal.toString().trimmed();
        if(text.isEmpty())
            return;

        messageReceived(sender,text);

    }else if(typeVal.toString().compare("newuser",Qt::CaseInsensitive) == 0){
        const QJsonValue usernameVal = docObj.value("username");
        if(usernameVal.isNull() || !usernameVal.isString())
            return;
        userJoined(usernameVal.toString());
    }
    else if (typeVal.toString().compare("userdisconnected",Qt::CaseInsensitive)==0){
        const QJsonValue usernameVal = docObj.value("username");
        if(usernameVal.isNull() || !usernameVal.isString())
            return;
        userLeft(usernameVal.toString());
    }else if(typeVal.toString().compare("userlist",Qt::CaseInsensitive)==0){
        // 收到用户列表，表示登录成功，切换页面
        if(ui->stackedWidget->currentWidget() != ui->chatPage) {
            ui->stackedWidget->setCurrentWidget(ui->chatPage);
        }

        const QJsonValue userlistVal = docObj.value("userlist");
        if(userlistVal.isNull() || !userlistVal.isArray())
            return;

        qDebug()<<userlistVal.toVariant().toStringList();
        userListReceived(userlistVal.toVariant().toStringList());
    }
}

void MainWindow::userJoined(const QString &user)
{
    ui->userListWidget->addItem(user);
}

void MainWindow::userLeft(const QString &user)
{
    for(auto aItem : ui->userListWidget ->findItems(user,Qt::MatchExactly)){
        qDebug("remove");
        ui->userListWidget->removeItemWidget(aItem);
        delete aItem;
    }
}

void MainWindow::userListReceived(const QStringList &list)
{
    ui->userListWidget->clear();
    ui->userListWidget->addItems(list);
}

