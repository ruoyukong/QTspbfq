#ifndef IDATABASE_H
#define IDATABASE_H

#include <QObject>
#include <QtSql>
#include <QSqlDatabase>
class IDatabase : public QObject
{
    Q_OBJECT
public:
    static IDatabase &getInstance()
    {
        static IDatabase instance;
        return instance;
    }

    QString userLogin(QString userName,QString password);




private:
    explicit IDatabase(QObject *parent = nullptr);
    IDatabase(IDatabase const &) = delete;
    void operator=(IDatabase const &) =delete;

    QSqlDatabase database;

    void ininDatabase();



signals:

public:
    bool initPatientMode1();
    int addNewPatient();
    bool searchPatient(QString filter);
    bool deleteCurrentPatient();
    bool submitPatientEdit();
    bool revertPatientEdit();

    QSqlTableModel *patientTabModel;
    QItemSelectionModel *thePatientSelection; //选择模型


};
#endif // DOCTOR_H
