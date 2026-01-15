#ifndef MYWIDGET_H
#define MYWIDGET_H

#include <QWidget>
#include <QMediaPlayer>
#include <QGraphicsView>
#include <QGraphicsVideoItem>
#include <QAudioOutput>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTableView>
#include <QTimer>
#include <QModelIndex>
#include <QAction>
#include <QUrl>
#include <QList>

// 前置声明（减少头文件包含，提升编译效率）
namespace Ui {
class MyWidget;
}

class PlaylistModel; // 对应自定义播放列表模型
struct MediaInfo;    // 对应媒体信息结构体（与原代码一致）

class MyWidget : public QWidget
{
    Q_OBJECT // Qt 信号槽机制必需宏

public:
    // 构造函数与析构函数
    explicit MyWidget(QWidget *parent = nullptr);
    ~MyWidget() override;

private slots:
    // ========== 核心功能槽函数（保留原功能） ==========
    void UpdatePlaybackProgress();  // 实时更新播放进度
    void UpdateTotalDuration();     // 更新视频总时长
    void TableClicked(const QModelIndex &index); // 播放列表点击事件
    void SetPaused();               // 播放/暂停切换
    void SkipBackward();            // 上一曲
    void SkipForward();             // 下一曲
    void SetPlayListShown();        // 显示/隐藏播放列表
    void importPlaylist();          // 导入播放列表
    void exportPlaylist();          // 导出播放列表
    void toggleFullScreen(bool checked); // 全屏切换
    void TrayIconActivated(QSystemTrayIcon::ActivationReason reason); // 托盘激活事件
    void showContextMenu(const QPoint &pos); // 显示上下文菜单

    // ========== 按钮槽函数（与 UI 绑定，保留原功能） ==========
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

private:
    // ========== 成员变量（与简化版 .cpp 对应，删除冗余变量） ==========
    Ui::MyWidget *ui;                      // UI 界面指针
    QMediaPlayer *mediaPlayer;             // 媒体播放器核心
    QGraphicsView *videoView;              // 视频显示视图
    QGraphicsVideoItem *videoItem;         // 视频显示项
    QAudioOutput *audioOutput;             // 音频输出设备
    int currentIndex;                      // 当前播放索引
    QSystemTrayIcon *tray_icon;            // 系统托盘图标
    QMenu *mainMenu;                       // 上下文主菜单
    QSlider *slider_brightness;            // 亮度调节滑块
    PlaylistModel *playlistModel;          // 播放列表模型
    QTableView *playlistView;              // 播放列表视图
    bool isFullScreenMode;                 // 全屏状态标记
    QTimer *progressUpdateTimer;           // 进度更新定时器
    QList<QUrl> sources;                   // 媒体文件路径列表（原代码保留）

    // ========== 私有成员函数（封装的初始化逻辑 + 核心功能函数） ==========
    // 封装的初始化函数（简化版新增，使构造函数清晰）
    void initLayoutSetting();              // 初始化布局（消除空白、设置拉伸比）
    void initVideoPlayer();                // 初始化视频播放器核心
    void initPlaylistView();               // 初始化播放列表视图
    void initTrayAndMenu();                // 初始化托盘和上下文菜单
    void initSignalConnection();           // 统一绑定信号槽
    void addCheckableAction(QMenu *menu, QActionGroup *group, const QString &text, bool checked); // 简化 Checkable Action 创建
    // mywidget.h 中，MyWidget 类的 private 部分添加
    int showCustomDialog(const QString &title, const QString &content, const QColor &textColor);
    // 核心功能函数（保留原功能，简化实现）
    void updateVideoBrightness();          // 更新视频亮度
    void updateVideoGeometry();            // 更新视频显示尺寸（自适应）
    QString getMediaDuration(const QUrl &mediaUrl); // 获取视频时长
    void logToFile(const QString &content); // 写入日志文件
    void change_action_state();            // 更新按钮可用状态
    void ClearSources();                   // 清空播放列表
    void PlayCurrent();                    // 播放当前索引视频
    void aspectChanged(QAction *action);   // 宽高比变更事件
    void scaleChanged(QAction *action);    // 缩放模式变更事件
    void viewResized(const QRect &rect);   // 视图尺寸变更事件

    // ========== 事件重写（保留原功能） ==========
    void closeEvent(QCloseEvent *event) override;  // 关闭事件（隐藏到托盘）
    void resizeEvent(QResizeEvent *event) override; // 窗口尺寸变更事件（自适应视频）
};

#endif // MYWIDGET_H
