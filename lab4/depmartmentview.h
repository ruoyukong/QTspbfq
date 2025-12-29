#ifndef DEPMARTMENTVIEW_H
#define DEPMARTMENTVIEW_H

#include <QWidget>

namespace Ui {
class DepmartmentView;
}

class DepmartmentView : public QWidget
{
    Q_OBJECT

public:
    explicit DepmartmentView(QWidget *parent = nullptr);
    ~DepmartmentView();

private:
    Ui::DepmartmentView *ui;
};

#endif // DEPMARTMENTVIEW_H
