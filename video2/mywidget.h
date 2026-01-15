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
#include <QResizeEvent>
#include <QPoint>
#include <QTime>
#include "playlistmodel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MyWidget; }
QT_END_NAMESPACE

class MyWidget : public QWidget
{
    Q_OBJECT

public:
    MyWidget(QWidget *parent = nullptr);
    ~MyWidget();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    // 自动关联槽函数
    void on_btLast_clicked();
    void on_btNext_clicked();
    void on_btStart_clicked();
    void on_btReset_clicked();
    void on_btUpload_clicked();
    void on_btList_clicked();
    void on_btExport_clicked();
    void on_btClose_clicked();
    void on_slider_valueChanged(int value);
    void on_times_valueChanged(int value);
    void on_lights_valueChanged(int value);

    // 自定义槽函数
    void UpdateTime();
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
    void toggleFullScreen(bool checked);
    void UpdatePlaybackProgress();
    void UpdateTotalDuration();

private:
    // 成员变量
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
    QWidget *brightnessOverlay;
    bool isFullScreenMode;

    // 工具函数
    QString getMediaDuration(const QUrl& mediaUrl);
    void logToFile(const QString &content);
    void PlayCurrent();
    void updateBrightnessOverlay();
    QTimer *progressUpdateTimer;
};

#endif // MYWIDGET_H
