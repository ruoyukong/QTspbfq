#ifndef WELCOMEVIEW_H
#define WELCOMEVIEW_H

#include <QWidget>

namespace Ui {
class WelcomeView;
}

class WelcomeView : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeView(QWidget *parent = nullptr);
    ~WelcomeView();

private slots:


    void on_btDeparment_clicked();

    void on_btDoctor_clicked();

    void on_btPatient_clicked();

signals:
    void goDepmartmentView();
    void goDoctor();
    void goPatientView();


private:
    Ui::WelcomeView *ui;
};

#endif // WELCOMEVIEW_H
