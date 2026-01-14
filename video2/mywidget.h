#ifndef MYWIDGET_H
#define MYWIDGET_H

#include <QWidget>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QList>
#include <QUrl>
#include <QSlider>
#include <QTableView>
#include <QCloseEvent>
#include <QWidget> // 确保 QWidget 可用
#include "playlistmodel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MyWidget; }
QT_END_NAMESPACE

class MyWidget : public QWidget
{
    Q_OBJECT

public:
    MyWidget(QWidget *parent = nullptr);
    ～MyWidget();

private slots:
    void UpdateTime(qint64);
    void on_slider_valueChanged(int value);
    void on_btLast_clicked();
    void on_btNext_clicked();
    void on_btStart_clicked();
    void on_btReset_clicked();
    void on_btUpload_clicked();
    void on_btList_clicked();
    void on_btExport_clicked() {} // 实际连接在构造函数中，可留空或删除
    void TableClicked(const QModelIndex &index);
    void SetPlayListShown();
    void change_action_state();
    void ClearSources();
    void SetPaused();
    void SkipBackward();
    void SkipForward();
    void showContextMenu(const QPoint &pos);
    void createContextMenu();
    void aspectChanged(QAction *action);
    void scaleChanged(QAction *action);
    void TrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void importPlaylist();
    void exportPlaylist();
    void closeEvent(QCloseEvent *event) override;

private:
    void PlayCurrent(); // 👈 新增

    Ui::MyWidget *ui;
    QMediaPlayer *mediaPlayer;
    QVideoWidget *videoWidget;
    QAudioOutput *audioOutput;
    int currentIndex;
    QList<QUrl> sources;
    QSystemTrayIcon *tray_icon;
    QMenu *mainMenu;
    double currentBrightness;
    QSlider *slider_brightness;
    QColor currentColor;
    PlaylistModel *playlistModel;
    QTableView *playlistView;

    // 替换为叠加层
    QWidget *brightnessOverlay = nullptr; // 👈 关键：不再用 QGraphicsColorizeEffect

    QString getMediaDuration(const QUrl& mediaUrl);
    void OpenFile();
    void logToFile(const QString &content);
};

#endif // MYWIDGET_H
