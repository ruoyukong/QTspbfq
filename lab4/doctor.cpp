#include "doctor.h"
#include "ui_doctor.h"

Doctor::Doctor(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Doctor)
{
    ui->setupUi(this);
}

Doctor::~Doctor()
{
    delete ui;
}
