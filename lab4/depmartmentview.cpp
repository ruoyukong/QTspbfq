#include "depmartmentview.h"
#include "ui_depmartmentview.h"

DepmartmentView::DepmartmentView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DepmartmentView)
{
    ui->setupUi(this);
}

DepmartmentView::~DepmartmentView()
{
    delete ui;
}
