#include "welcomeview.h"
#include "ui_welcomeview.h"
#include <QDebug>

WelcomeView::WelcomeView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WelcomeView)
{
    qDebug()<<"create WelcomeView";
    ui->setupUi(this);
}

WelcomeView::~WelcomeView()
{
    qDebug()<<"destroy WelcomeView";
    delete ui;
}



void WelcomeView::on_btDeparment_clicked()
{
    emit goDepmartmentView();
}


void WelcomeView::on_btDoctor_clicked()
{
    emit goDoctor();
}


void WelcomeView::on_btPatient_clicked()
{
    emit goPatientView();
}

