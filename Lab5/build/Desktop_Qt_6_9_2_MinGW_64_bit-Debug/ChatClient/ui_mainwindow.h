/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.9.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QLabel *titleLabel;
    QStackedWidget *stackedWidget;
    QWidget *loginPage;
    QFrame *loeinFrame;
    QWidget *widget;
    QVBoxLayout *verticalLayout;
    QGridLayout *gridLayout;
    QLabel *label_2;
    QLineEdit *serverEdit;
    QLabel *label_3;
    QLineEdit *userName;
    QSpacerItem *verticalSpacer;
    QPushButton *loginButton;
    QWidget *chatPage;
    QWidget *widget1;
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *horizontalLayout;
    QTextEdit *roomTexitEdit;
    QListWidget *userListWidget;
    QHBoxLayout *horizontalLayout_2;
    QLineEdit *sayLineEdit;
    QPushButton *sayButton;
    QPushButton *logoutButton;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(708, 404);
        MainWindow->setStyleSheet(QString::fromUtf8("#titleLabel{\n"
"background: white;\n"
"color :blue;\n"
"font-size: 20px;\n"
"border:none;\n"
"border-bottom: 1px solid black;\n"
"padding: 5px;\n"
"font\357\274\23224pt\"\351\273\221\344\275\223\";\n"
"color:rgb(255,255,255);\n"
"background-color:rgba(0,170,127,255);\n"
"}\n"
"\n"
"#mainFrame{\n"
"border:none;\n"
"background: white;\n"
"}\n"
"\n"
"#loeinFrame{\n"
"background: qlineargradient(x1:0, y1:0, x2:0, y2: 1,stop:0#ddf,stop: 1 #aaf);\n"
"border: 1px soid gray;\n"
"padding: 10px;\n"
"borderradius: 25px;\n"
"}\n"
""));
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        titleLabel = new QLabel(centralwidget);
        titleLabel->setObjectName("titleLabel");
        titleLabel->setGeometry(QRect(10, 10, 431, 31));
        stackedWidget = new QStackedWidget(centralwidget);
        stackedWidget->setObjectName("stackedWidget");
        stackedWidget->setGeometry(QRect(10, 40, 611, 321));
        loginPage = new QWidget();
        loginPage->setObjectName("loginPage");
        loeinFrame = new QFrame(loginPage);
        loeinFrame->setObjectName("loeinFrame");
        loeinFrame->setGeometry(QRect(80, 60, 411, 181));
        loeinFrame->setStyleSheet(QString::fromUtf8("#titleLabel{\n"
"background: white;\n"
"color :blue;\n"
"font-size: 20px;\n"
"border:none;\n"
"border-bottom: 1px solid black;\n"
"padding: 5ps;\n"
"}\n"
"\n"
"#mainFrame{\n"
"border:none;\n"
"background: white;\n"
"}\n"
"\n"
"#loeinFrame{\n"
"background: qlineargradient(x1:0, y1:0, x2:0, y2: 1,stop:0#ddf,stop: 1 #aaf);\n"
"border: 1px soid gray;\n"
"padding: 10px;\n"
"borderradius: 25px;\n"
"}\n"
""));
        loeinFrame->setFrameShape(QFrame::Shape::StyledPanel);
        loeinFrame->setFrameShadow(QFrame::Shadow::Raised);
        widget = new QWidget(loeinFrame);
        widget->setObjectName("widget");
        widget->setGeometry(QRect(70, 30, 261, 131));
        verticalLayout = new QVBoxLayout(widget);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        gridLayout = new QGridLayout();
        gridLayout->setObjectName("gridLayout");
        label_2 = new QLabel(widget);
        label_2->setObjectName("label_2");

        gridLayout->addWidget(label_2, 0, 0, 1, 1);

        serverEdit = new QLineEdit(widget);
        serverEdit->setObjectName("serverEdit");

        gridLayout->addWidget(serverEdit, 0, 1, 1, 1);

        label_3 = new QLabel(widget);
        label_3->setObjectName("label_3");

        gridLayout->addWidget(label_3, 1, 0, 1, 1);

        userName = new QLineEdit(widget);
        userName->setObjectName("userName");

        gridLayout->addWidget(userName, 1, 1, 1, 1);


        verticalLayout->addLayout(gridLayout);

        verticalSpacer = new QSpacerItem(10, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        loginButton = new QPushButton(widget);
        loginButton->setObjectName("loginButton");

        verticalLayout->addWidget(loginButton);

        stackedWidget->addWidget(loginPage);
        chatPage = new QWidget();
        chatPage->setObjectName("chatPage");
        widget1 = new QWidget(chatPage);
        widget1->setObjectName("widget1");
        widget1->setGeometry(QRect(30, 20, 541, 291));
        verticalLayout_2 = new QVBoxLayout(widget1);
        verticalLayout_2->setObjectName("verticalLayout_2");
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        roomTexitEdit = new QTextEdit(widget1);
        roomTexitEdit->setObjectName("roomTexitEdit");

        horizontalLayout->addWidget(roomTexitEdit);

        userListWidget = new QListWidget(widget1);
        userListWidget->setObjectName("userListWidget");
        userListWidget->setMaximumSize(QSize(120, 16777215));

        horizontalLayout->addWidget(userListWidget);


        verticalLayout_2->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        sayLineEdit = new QLineEdit(widget1);
        sayLineEdit->setObjectName("sayLineEdit");

        horizontalLayout_2->addWidget(sayLineEdit);

        sayButton = new QPushButton(widget1);
        sayButton->setObjectName("sayButton");

        horizontalLayout_2->addWidget(sayButton);

        logoutButton = new QPushButton(widget1);
        logoutButton->setObjectName("logoutButton");

        horizontalLayout_2->addWidget(logoutButton);


        verticalLayout_2->addLayout(horizontalLayout_2);

        stackedWidget->addWidget(chatPage);
        MainWindow->setCentralWidget(centralwidget);

        retranslateUi(MainWindow);

        stackedWidget->setCurrentIndex(1);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "\350\201\212\345\244\251\345\256\244\345\256\242\346\210\267\347\253\257", nullptr));
        titleLabel->setText(QCoreApplication::translate("MainWindow", "\351\231\210\346\200\235\345\256\207\347\232\204\350\201\212\345\244\251\345\256\244", nullptr));
        label_2->setText(QCoreApplication::translate("MainWindow", "\346\234\215\345\212\241\345\231\250\345\234\260\345\235\200\357\274\232", nullptr));
        label_3->setText(QCoreApplication::translate("MainWindow", "\346\230\265\347\247\260\357\274\232", nullptr));
        loginButton->setText(QCoreApplication::translate("MainWindow", "\347\231\273\345\205\245", nullptr));
        sayButton->setText(QCoreApplication::translate("MainWindow", "\345\217\221\351\200\201", nullptr));
        logoutButton->setText(QCoreApplication::translate("MainWindow", "\351\200\200\345\207\272", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
