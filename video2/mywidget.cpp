#include "mywidget.h"
#include "ui_mywidget.h"
#include "playlistmodel.h" // 保留对现有 playlistmodel.h 的引用
#include <QActionGroup>
#include <QFileDialog>
#include <QIcon>
#include <QStandardPaths>
#include <QTime>
#include <QVBoxLayout>
#include <QSlider>
#include <QMessageBox>
#include <QDebug>
#include <QPalette>
#include <QColor>
#include <QtConcurrent/QtConcurrent>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QSemaphore>
#include <QResizeEvent>
#include <QCloseEvent>

// 全局信号量：限制同时获取时长的线程数
static QSemaphore durationFetchSemaphore(2);

// 构造函数实现
MyWidget::MyWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MyWidget)
    , mediaPlayer(new QMediaPlayer(this))
    , videoWidget(new QVideoWidget(this)) // 直接使用原生 QVideoWidget
    , audioOutput(new QAudioOutput(this))
    , currentIndex(-1)
    , tray_icon(nullptr)
    , mainMenu(nullptr)
    , currentBrightness(1.0)
    , slider_brightness(nullptr)
    , currentColor(Qt::white)
    , playlistModel(new PlaylistModel(this))
    , playlistView(new QTableView(this))
    , brightnessOverlay(new QWidget(videoWidget)) // 父控件设为 videoWidget
{
    ui->setupUi(this);

    setWindowTitle(tr("士麦那视频播放器"));
    setWindowIcon(QIcon(":/images/icon.png"));

    // ========== 核心优化：消除所有页面空白，界面完全占满 ==========
    // 1. 主窗口布局无内边距、无间距
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (mainLayout) {
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
    }

    // 2. videoWidget 拉伸填充，无背景空白
    videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoWidget->setStyleSheet("background-color: black;");
    videoWidget->setContentsMargins(0, 0, 0, 0);

    // 3. tableWidget 布局无内边距、无间距，承载videoWidget
    QVBoxLayout *videoLayout = new QVBoxLayout(ui->tableWidget);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(0);
    videoLayout->addWidget(videoWidget);
    ui->tableWidget->setLayout(videoLayout);
    ui->tableWidget->setContentsMargins(0, 0, 0, 0);

    // 4. 下方控件布局优化，减少冗余空白（保留最小间距，无多余内边距）
    QVBoxLayout *bottomLayout = qobject_cast<QVBoxLayout*>(ui->verticalLayout);
    if (bottomLayout) {
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setSpacing(2);
    }
    ui->verticalLayout->setContentsMargins(0, 0, 0, 0);

    // 5. 设置拉伸比例，确保视频区域占满绝大部分，下方控件不占用多余空间
    if (mainLayout) {
        mainLayout->setStretchFactor(ui->tableWidget, 99);
        mainLayout->setStretchFactor(ui->verticalLayout, 1);
    }

    // ========== 亮度叠加层：自动填满videoWidget，无空白，无需resize监听 ==========
    brightnessOverlay->setGeometry(videoWidget->rect());
    brightnessOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    brightnessOverlay->setStyleSheet("background-color: rgba(0,0,0,0);");
    brightnessOverlay->setContentsMargins(0, 0, 0, 0);
    // 关键：设置布局使其自动拉伸，无空白
    QVBoxLayout *overlayLayout = new QVBoxLayout(brightnessOverlay);
    overlayLayout->setContentsMargins(0, 0, 0, 0);
    overlayLayout->setSpacing(0);
    brightnessOverlay->setLayout(overlayLayout);
    brightnessOverlay->show(); // 始终显示，靠透明度控制

    // ========== 播放器初始化 ==========
    mediaPlayer->setVideoOutput(videoWidget);
    mediaPlayer->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.5);

    // ========== 播放进度滑块（直接关联值变化，无手动connect）==========
    QSlider *seek_slider = ui->times;
    seek_slider->setRange(0, 1000);
    seek_slider->setContentsMargins(0, 0, 0, 0);

    // ========== 亮度滑块初始化 ==========
    slider_brightness = ui->lights;
    slider_brightness->setRange(-100, 100);
    slider_brightness->setValue(0);
    slider_brightness->setContentsMargins(0, 0, 0, 0);

    // ========== 播放列表 ==========
    playlistView->setModel(playlistModel);
    playlistView->setColumnWidth(0, 200);
    playlistView->setWindowTitle(tr("播放列表"));
    playlistView->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    playlistView->resize(400, 400);
    playlistView->setSelectionMode(QAbstractItemView::SingleSelection);
    playlistView->setSelectionBehavior(QAbstractItemView::SelectRows);
    playlistView->setShowGrid(false);
    playlistView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    playlistView->setContextMenuPolicy(Qt::CustomContextMenu);
    playlistView->setContentsMargins(0, 0, 0, 0);

    // ========== 系统托盘 ==========
    tray_icon = new QSystemTrayIcon(QIcon(":/images/icon.png"), this);
    tray_icon->setToolTip(tr("士麦那视频播放器"));
    QMenu *tray_menu = new QMenu(this);
    tray_menu->addAction(tr("播放"), this, &MyWidget::SetPaused);
    tray_menu->addAction(tr("停止"), mediaPlayer, &QMediaPlayer::stop);
    tray_menu->addAction(tr("上一个"), this, &MyWidget::SkipBackward);
    tray_menu->addAction(tr("下一个"), this, &MyWidget::SkipForward);
    tray_menu->addSeparator();
    tray_menu->addAction(tr("播放列表"), this, &MyWidget::SetPlayListShown);
    tray_menu->addSeparator();
    tray_menu->addAction(tr("退出"), qApp, &QApplication::quit);
    tray_icon->setContextMenu(tray_menu);
    tray_icon->show();

    // ========== 上下文菜单 ==========
    setContextMenuPolicy(Qt::CustomContextMenu);
    videoWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    createContextMenu();

    // ========== 按钮初始状态 ==========
    ui->btStart->setEnabled(false);
    ui->btReset->setEnabled(false);
    ui->btLast->setEnabled(false);
    ui->btNext->setEnabled(false);
}

// 析构函数
MyWidget::~MyWidget()
{
    delete ui;
}

// ========== 工具函数（保持不变，兼容现有 playlistmodel.h）==========
QString MyWidget::getMediaDuration(const QUrl& mediaUrl)
{
    QMediaPlayer tempPlayer;
    tempPlayer.setSource(mediaUrl);

    QEventLoop loop;
    QObject::connect(&tempPlayer, &QMediaPlayer::metaDataChanged, &loop, &QEventLoop::quit);
    QTimer::singleShot(800, &loop, &QEventLoop::quit);
    loop.exec();

    qint64 duration = tempPlayer.duration();
    if (duration <= 0) return "00:00";
    int seconds = (duration / 1000) % 60;
    int minutes = (duration / 60000) % 60;
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

void MyWidget::logToFile(const QString &content)
{
    QtConcurrent::run([content]() {
        QFile file("player_log.txt");
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                   << " - " << content << "\n";
            file.close();
        }
    });
}

void MyWidget::PlayCurrent()
{
    if (currentIndex < 0 || currentIndex >= sources.size()) return;
    mediaPlayer->setSource(sources[currentIndex]);
    mediaPlayer->play();
    ui->btStart->setText(tr("暂停"));
    playlistView->selectRow(currentIndex);
    change_action_state();
    logToFile(QString("播放视频：") + sources[currentIndex].toLocalFile());
}

void MyWidget::TableClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    currentIndex = index.row();
    PlayCurrent();
}

void MyWidget::ClearSources()
{
    sources.clear();
    playlistModel->clear(); // 直接调用现有 playlistmodel.h 中的 clear 方法
    currentIndex = -1;
    mediaPlayer->stop();
    ui->btStart->setText(tr("播放"));
    change_action_state();
    logToFile("清空播放列表");
}

void MyWidget::createContextMenu()
{
    mainMenu = new QMenu(this);

    QMenu *aspectMenu = mainMenu->addMenu(tr("宽高比"));
    QActionGroup *aspectGroup = new QActionGroup(aspectMenu);
    aspectGroup->setExclusive(true);
    auto addAction = [&](const QString& text, bool checked = false) {
        QAction *act = aspectMenu->addAction(text);
        act->setCheckable(true);
        act->setChecked(checked);
        aspectGroup->addAction(act);
        return act;
    };
    addAction(tr("自动"), true);
    addAction(tr("16:9"));
    addAction(tr("4:3"));

    QMenu *scaleMenu = mainMenu->addMenu(tr("缩放模式"));
    QActionGroup *scaleGroup = new QActionGroup(scaleMenu);
    scaleGroup->setExclusive(true);
    auto addScaleAction = [&](const QString& text, bool checked = false) {
        QAction *act = scaleMenu->addAction(text);
        act->setCheckable(true);
        act->setChecked(checked);
        scaleGroup->addAction(act);
        return act;
    };
    addScaleAction(tr("不缩放"), true);
    addScaleAction(tr("1.2倍缩放"));

    QAction *fullScreenAction = mainMenu->addAction(tr("全屏"));
    fullScreenAction->setCheckable(true);

    mainMenu->addSeparator();
    mainMenu->addAction(tr("导入播放列表"), this, &MyWidget::importPlaylist);
    mainMenu->addAction(tr("导出播放列表"), this, &MyWidget::exportPlaylist);
}

void MyWidget::SetPaused()
{
    if (mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        mediaPlayer->pause();
        ui->btStart->setText(tr("播放"));
    } else {
        mediaPlayer->play();
        ui->btStart->setText(tr("暂停"));
    }
    ui->btReset->setEnabled(true);
    logToFile(QString("切换播放状态：") + (mediaPlayer->playbackState() == QMediaPlayer::PlayingState ? "播放" : "暂停"));
}

void MyWidget::SkipBackward()
{
    if (sources.isEmpty() || currentIndex <= 0) return;
    currentIndex--;
    PlayCurrent();
}

void MyWidget::SkipForward()
{
    if (sources.isEmpty() || currentIndex >= static_cast<int>(sources.size()) - 1) return;
    currentIndex++;
    PlayCurrent();
}

void MyWidget::SetPlayListShown()
{
    if (playlistView->isHidden()) {
        playlistView->move(frameGeometry().bottomLeft());
        playlistView->show();
    } else {
        playlistView->hide();
    }
}

void MyWidget::change_action_state()
{
    bool hasMedia = !sources.isEmpty();
    ui->btStart->setEnabled(hasMedia);
    ui->btReset->setEnabled(hasMedia);
    ui->btLast->setEnabled(hasMedia && currentIndex > 0);
    ui->btNext->setEnabled(hasMedia && currentIndex < static_cast<int>(sources.size()) - 1);
}

void MyWidget::UpdateTime()
{
    qint64 total = mediaPlayer->duration();
    qint64 pos = mediaPlayer->position();
    QTime tTotal(0, (total / 60000) % 60, (total / 1000) % 60);
    QTime tCurrent(0, (pos / 60000) % 60, (pos / 1000) % 60);
    ui->time->setText(tCurrent.toString("mm:ss"));
    ui->totaltime->setText(tTotal.toString("mm:ss"));
}

void MyWidget::showContextMenu(const QPoint &pos)
{
    mainMenu->popup(videoWidget->isFullScreen() ? pos : mapToGlobal(pos));
}

void MyWidget::aspectChanged(QAction *action)
{
    if (action->text() == tr("16:9") || action->text() == tr("4:3")) {
        videoWidget->setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
    } else {
        videoWidget->setAspectRatioMode(Qt::IgnoreAspectRatio);
    }
}

void MyWidget::scaleChanged(QAction *action)
{
    QSize originalSize = videoWidget->sizeHint().isEmpty() ? QSize(640, 480) : videoWidget->sizeHint();
    if (action->text() == tr("1.2倍缩放")) {
        videoWidget->resize(qRound(originalSize.width() * 1.2), qRound(originalSize.height() * 1.2));
    } else {
        videoWidget->resize(originalSize);
    }
}

void MyWidget::TrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        showNormal();
        activateWindow();
    }
}

void MyWidget::importPlaylist()
{
    QString path = QFileDialog::getOpenFileName(this, tr("导入播放列表"), "", "文本文件 (*.txt)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法打开文件"));
        return;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (!line.isEmpty() && QFile::exists(line)) {
            QUrl url = QUrl::fromLocalFile(line);
            QFileInfo fi(line);
            QtConcurrent::run([this, url, fi]() {
                durationFetchSemaphore.acquire();
                QString duration = getMediaDuration(url);
                durationFetchSemaphore.release();

                // 直接使用 playlistmodel.h 中的 MediaInfo 结构体，无冗余定义
                MediaInfo info{url, fi.baseName(), duration};
                QMetaObject::invokeMethod(this, [this, info]() {
                    playlistModel->addMedia(info); // 调用现有 playlistmodel.h 中的 addMedia 方法
                    sources.append(info.url);
                });
            });
        }
    }
    file.close();
    logToFile(QString("导入播放列表：") + path);
    change_action_state();
}

void MyWidget::exportPlaylist()
{
    QString path = QFileDialog::getSaveFileName(this, tr("导出播放列表"), "", "文本文件 (*.txt)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法保存文件"));
        return;
    }

    QTextStream stream(&file);
    for (int i = 0; i < playlistModel->mediaCount(); ++i) {
        // 调用现有 playlistmodel.h 中的 mediaAt 方法
        stream << playlistModel->mediaAt(i).url.toLocalFile() << "\n";
    }
    file.close();
    logToFile(QString("导出播放列表：") + path);
}

// ========== 按钮槽函数（所有功能内联，无手动connect，兼容现有 playlistmodel.h）==========
void MyWidget::on_btLast_clicked()
{
    // 上一个视频功能
    if (sources.isEmpty() || currentIndex <= 0) return;
    currentIndex--;
    PlayCurrent();
}

void MyWidget::on_btNext_clicked()
{
    // 下一个视频功能
    if (sources.isEmpty() || currentIndex >= static_cast<int>(sources.size()) - 1) return;
    currentIndex++;
    PlayCurrent();
}

void MyWidget::on_btStart_clicked()
{
    // 播放/暂停切换功能
    if (mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        mediaPlayer->pause();
        ui->btStart->setText(tr("播放"));
    } else {
        mediaPlayer->play();
        ui->btStart->setText(tr("暂停"));
    }
    ui->btReset->setEnabled(true);
    logToFile(QString("切换播放状态：") + (mediaPlayer->playbackState() == QMediaPlayer::PlayingState ? "播放" : "暂停"));
}

void MyWidget::on_btReset_clicked()
{
    // 停止播放功能
    mediaPlayer->stop();
    ui->btStart->setText(tr("播放"));
}

void MyWidget::on_btUpload_clicked()
{
    // 打开文件功能
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("打开媒体文件"),
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
        "视频文件 (*.mp4 *.avi *.mkv *.mov)"
        );
    if (files.isEmpty()) return;

    int oldCount = sources.size();
    for (const QString &f : files) {
        QUrl url = QUrl::fromLocalFile(f);
        sources.append(url);
        QFileInfo fi(f);

        QtConcurrent::run([this, url, fi]() {
            durationFetchSemaphore.acquire();
            QString duration = getMediaDuration(url);
            durationFetchSemaphore.release();

            // 直接使用 playlistmodel.h 中的 MediaInfo 结构体
            MediaInfo info{url, fi.baseName(), duration};
            QMetaObject::invokeMethod(this, [this, info]() {
                playlistModel->addMedia(info); // 调用现有 playlistmodel.h 中的 addMedia 方法
            });
        });
        logToFile(QString("打开文件：") + f);
    }

    if (oldCount == 0 && !sources.isEmpty()) {
        currentIndex = 0;
        PlayCurrent();
    }
    change_action_state();
}

void MyWidget::on_btList_clicked()
{
    // 显示/隐藏播放列表功能
    if (playlistView->isHidden()) {
        playlistView->move(frameGeometry().bottomLeft());
        playlistView->show();
    } else {
        playlistView->hide();
    }
}

void MyWidget::on_btExport_clicked()
{
    // 导出播放列表功能（内联实现，调用现有 playlistmodel.h 接口）
    QString path = QFileDialog::getSaveFileName(this, tr("导出播放列表"), "", "文本文件 (*.txt)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法保存文件"));
        return;
    }

    QTextStream stream(&file);
    for (int i = 0; i < playlistModel->mediaCount(); ++i) {
        // 调用现有 playlistmodel.h 中的 mediaAt 方法
        stream << playlistModel->mediaAt(i).url.toLocalFile() << "\n";
    }
    file.close();
    logToFile(QString("导出播放列表：") + path);
}

void MyWidget::on_btClose_clicked()
{
    // 关闭程序功能（内联实现）
    qApp->quit();
}

void MyWidget::on_slider_valueChanged(int value)
{
    // 音量调节功能（内联实现）
    double volume = static_cast<double>(value) / 100.0;
    audioOutput->setVolume(volume);
}

void MyWidget::on_times_valueChanged(int value)
{
    // 播放进度调节功能（内联实现，对应times滑块）
    if (mediaPlayer->duration() > 0) {
        mediaPlayer->setPosition(static_cast<qint64>(value * mediaPlayer->duration() / 1000.0));
    }
}

void MyWidget::on_lights_valueChanged(int value)
{
    // 亮度调节功能（内联实现，对应lights滑块）
    if (value == 0) {
        brightnessOverlay->setStyleSheet("background-color: rgba(0,0,0,0);");
    } else if (value < 0) {
        int alpha = qMin(255, static_cast<int>(qAbs(value) * 2.55));
        brightnessOverlay->setStyleSheet(QString("background-color: rgba(0,0,0,%1);").arg(alpha));
    } else {
        int alpha = qMin(255, static_cast<int>(value * 2.55));
        brightnessOverlay->setStyleSheet(QString("background-color: rgba(255,255,255,%1);").arg(alpha));
    }
}

void MyWidget::closeEvent(QCloseEvent *event)
{
    hide();
    tray_icon->showMessage(tr("士麦那视频播放器"), tr("单击我重新回到主界面"), QSystemTrayIcon::Information, 2000);
    event->ignore();
}
