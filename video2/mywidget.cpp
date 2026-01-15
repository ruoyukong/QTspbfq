#include "mywidget.h"
#include "ui_mywidget.h"
#include "playlistmodel.h"
#include <QActionGroup>
#include <QFileDialog>
#include <QIcon>
#include <QStandardPaths>
#include <QTime>
#include <QVBoxLayout>
#include <QSlider>
#include <QMessageBox>
#include <QPalette>
#include <QColor>
#include <QtConcurrent/QtConcurrent>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QSemaphore>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QTimer>
#include <QGraphicsScene>
#include <QGraphicsColorizeEffect>
#include <QGraphicsView>

// 全局信号量：限制同时获取时长的线程数（保留核心，无冗余）
static QSemaphore durationFetchSemaphore(2);

// 构造函数实现（简化：封装零散初始化，删除冗余变量）
MyWidget::MyWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MyWidget)
    , mediaPlayer(new QMediaPlayer(this))
    , videoView(new QGraphicsView(this))
    , videoItem(new QGraphicsVideoItem())
    , audioOutput(new QAudioOutput(this))
    , currentIndex(-1)
    , tray_icon(nullptr)
    , mainMenu(nullptr)
    , slider_brightness(nullptr)  // 移除无实际作用的 currentBrightness、currentColor
    , playlistModel(new PlaylistModel(this))
    , playlistView(new QTableView(this))
    , isFullScreenMode(false)
    , progressUpdateTimer(new QTimer(this))
{
    ui->setupUi(this);
    setWindowTitle(tr("士麦那视频播放器"));

    // 封装零散初始化逻辑，使构造函数更清晰
    initLayoutSetting();    // 布局初始化（消除空白、设置拉伸比）
    initVideoPlayer();      // 播放器核心初始化
    initPlaylistView();     // 播放列表初始化
    initTrayAndMenu();      // 托盘和上下文菜单初始化
    initSignalConnection(); // 信号槽统一绑定

    // 初始按钮状态（保留原功能，无冗余）
    ui->btStart->setEnabled(false);
    ui->btReset->setEnabled(false);
    ui->btLast->setEnabled(false);
    ui->btNext->setEnabled(false);

    // 初始亮度设置（保留原功能）
    updateVideoBrightness();
}

// 析构函数（无冗余，保留原功能）
MyWidget::~MyWidget()
{
    delete ui;
}

// ========== 封装：布局初始化（合并所有重复的布局设置，删除冗余判断） ==========
void MyWidget::initLayoutSetting()
{
    // 主布局：消除空白、设置拉伸比例（合并原分散的 mainLayout 操作）
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (mainLayout) {
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        mainLayout->setStretchFactor(ui->widget1, 99);
        mainLayout->setStretchFactor(ui->verticalLayout, 1);
    }

    // 视频播放区域：QGraphicsView 初始化（合并原分散的设置，删除冗余尺寸赋值）
    videoView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoView->setStyleSheet("background-color: black; border: none;");
    videoView->setContentsMargins(0, 0, 0, 0);
    videoView->setRenderHint(QPainter::Antialiasing, false);
    videoView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    videoView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    videoView->setFrameShape(QFrame::NoFrame);
    videoView->setAlignment(Qt::AlignCenter);

    // 视频场景初始化（保留原功能，无冗余）
    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->addItem(videoItem);
    videoView->setScene(scene);

    // 视频区域放入 widget1（合并原分散的 videoLayout 操作）
    QVBoxLayout *videoLayout = new QVBoxLayout(ui->widget1);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(0);
    videoLayout->addWidget(videoView);
    videoLayout->setStretchFactor(videoView, 1);

    // 底部控制栏布局（保留原功能，简化判断）
    QVBoxLayout *bottomLayout = qobject_cast<QVBoxLayout*>(ui->verticalLayout);
    if (bottomLayout) {
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setSpacing(2);
    }
}

// ========== 封装：播放器核心初始化（合并原分散的播放器、滑块设置） ==========
void MyWidget::initVideoPlayer()
{
    // 播放器核心设置（保留原功能）
    mediaPlayer->setVideoOutput(videoItem);
    mediaPlayer->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.5);

    // 进度滑块（删除冗余的 seek_slider 变量，直接操作 ui 组件）
    ui->times->setRange(0, 1000);
    ui->times->setContentsMargins(0, 0, 0, 0);

    // 亮度滑块（保留原功能，移除无作用的变量）
    slider_brightness = ui->lights;
    slider_brightness->setRange(-100, 100);
    slider_brightness->setValue(0);
    slider_brightness->setContentsMargins(0, 0, 0, 0);

    // 进度更新定时器（保留原功能，无冗余）
    progressUpdateTimer->setInterval(50);
}

// ========== 封装：播放列表初始化（合并样式、模型绑定，简化代码） ==========
void MyWidget::initPlaylistView()
{
    // 播放列表样式（简化样式表，合并原分散的 palette 设置）
    playlistView->setStyleSheet(R"(
        QTableView { background-color: white; color: black; border: 1px solid #cccccc; gridline-color: #eeeeee; }
        QTableView::header { background-color: #f5f5f5; color: black; }
        QTableView::item:selected { background-color: #add8e6; color: black; }
    )");

    QPalette playlistPalette = playlistView->palette();
    playlistPalette.setColor(QPalette::Base, Qt::white);
    playlistPalette.setColor(QPalette::Text, Qt::black);
    playlistPalette.setColor(QPalette::Highlight, QColor(173, 216, 230));
    playlistPalette.setColor(QPalette::HighlightedText, Qt::black);
    playlistView->setPalette(playlistPalette);
    playlistView->setAttribute(Qt::WA_OpaquePaintEvent, true);

    // 播放列表核心设置（保留原功能，简化窗口标志）
    playlistView->setModel(playlistModel);
    playlistView->setColumnWidth(0, 200);
    playlistView->setWindowTitle(tr("播放列表"));
    playlistView->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    playlistView->resize(400, 400);
    playlistView->setSelectionMode(QAbstractItemView::SingleSelection);
    playlistView->setSelectionBehavior(QAbstractItemView::SelectRows);
    playlistView->setShowGrid(false);
    playlistView->setContextMenuPolicy(Qt::CustomContextMenu);
}

// ========== 封装：托盘和上下文菜单（合并原分散的创建逻辑，简化 Action 创建） ==========
void MyWidget::initTrayAndMenu()
{
    // 系统托盘（保留原功能，无冗余）
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

    // 上下文菜单（简化重复的 Action 创建逻辑）
    mainMenu = new QMenu(this);
    setContextMenuPolicy(Qt::CustomContextMenu);
    videoView->setContextMenuPolicy(Qt::CustomContextMenu);

    // 宽高比菜单（封装重复创建逻辑，减少冗余）
    QMenu *aspectMenu = mainMenu->addMenu(tr("宽高比"));
    QActionGroup *aspectGroup = new QActionGroup(aspectMenu);
    aspectGroup->setExclusive(true);
    addCheckableAction(aspectMenu, aspectGroup, tr("自动"), true);
    addCheckableAction(aspectMenu, aspectGroup, tr("16:9"),false);


    // 缩放模式菜单（同上，简化重复代码）
    QMenu *scaleMenu = mainMenu->addMenu(tr("缩放模式"));
    QActionGroup *scaleGroup = new QActionGroup(scaleMenu);
    scaleGroup->setExclusive(true);
    addCheckableAction(scaleMenu, scaleGroup, tr("不缩放"), true);
    addCheckableAction(scaleMenu, scaleGroup, tr("1.2倍缩放"),false);

    // 全屏 Action（保留原功能）
    QAction *fullScreenAction = mainMenu->addAction(tr("全屏"));
    fullScreenAction->setCheckable(true);
    connect(fullScreenAction, &QAction::toggled, this, &MyWidget::toggleFullScreen);

    // 导入导出 Action（保留原功能）
    mainMenu->addSeparator();
    mainMenu->addAction(tr("导入播放列表"), this, &MyWidget::importPlaylist);
    mainMenu->addAction(tr("导出播放列表"), this, &MyWidget::exportPlaylist);
}

// ========== 工具函数：简化重复的 Checkable Action 创建（减少冗余代码） ==========
void MyWidget::addCheckableAction(QMenu *menu, QActionGroup *group, const QString &text, bool checked)
{
    QAction *act = menu->addAction(text);
    act->setCheckable(true);
    act->setChecked(checked);
    group->addAction(act);
}

// ========== 封装：信号槽统一绑定（合并原分散的 connect，逻辑更清晰） ==========
void MyWidget::initSignalConnection()
{
    // 播放列表点击信号
    connect(playlistView, &QTableView::clicked, this, &MyWidget::TableClicked);

    // 进度更新相关信号
    connect(progressUpdateTimer, &QTimer::timeout, this, &MyWidget::UpdatePlaybackProgress);
    connect(mediaPlayer, &QMediaPlayer::durationChanged, this, &MyWidget::UpdateTotalDuration);
    connect(mediaPlayer, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::PlayingState) {
            progressUpdateTimer->start();
        } else {
            progressUpdateTimer->stop();
        }
    });

    // 托盘激活信号
    connect(tray_icon, &QSystemTrayIcon::activated, this, &MyWidget::TrayIconActivated);
}

// ========== 亮度调节（简化：删除冗余变量，合并条件判断，保留原功能） ==========
void MyWidget::updateVideoBrightness()
{
    int value = slider_brightness->value();
    videoItem->setGraphicsEffect(nullptr); // 移除已有效果

    if (value != 0) {
        QGraphicsColorizeEffect *effect = new QGraphicsColorizeEffect(this);
        // 合并颜色和强度设置，减少冗余代码
        effect->setColor(value < 0 ? Qt::black : Qt::white);
        float strength = qAbs(value) / 100.0f;
        effect->setStrength(value < 0 ? strength * 0.8f : strength * 0.5f);

        videoItem->setGraphicsEffect(effect);
    }
}

// ========== 更新视频尺寸（简化：删除冗余变量，简化计算逻辑，保留原功能） ==========
void MyWidget::updateVideoGeometry()
{
    if (!videoItem || !videoView || !videoView->scene()) return;

    // 简化默认尺寸赋值，减少冗余变量
    QSizeF videoSize = videoItem->nativeSize().isEmpty() ? QSizeF(640, 480) : videoItem->nativeSize();
    QRectF viewRect = videoView->viewport()->rect();
    if (viewRect.isEmpty()) return;

    // 简化宽高比计算，合并条件判断
    qreal videoAspect = videoSize.width() / videoSize.height();
    qreal viewAspect = viewRect.width() / viewRect.height();
    QSizeF targetSize = videoAspect > viewAspect
                            ? QSizeF(viewRect.width(), viewRect.width() / videoAspect)
                            : QSizeF(viewRect.height() * videoAspect, viewRect.height());

    // 简化居中设置，删除冗余变量
    videoItem->setSize(targetSize);
    videoItem->setPos(viewRect.center() - videoItem->boundingRect().center());
    videoView->update();
}

// ========== 进度实时更新（简化：删除冗余变量，简化时间赋值，保留原功能） ==========
void MyWidget::UpdatePlaybackProgress()
{
    if (!mediaPlayer || mediaPlayer->duration() <= 0) return;

    qint64 currentPos = mediaPlayer->position();
    qint64 totalDuration = mediaPlayer->duration();
    int sliderValue = static_cast<int>((static_cast<qint64>(1000) * currentPos) / totalDuration);

    // 进度滑块更新（保留原功能，无冗余）
    if (ui->times->value() != sliderValue) {
        ui->times->blockSignals(true);
        ui->times->setValue(sliderValue);
        ui->times->blockSignals(false);
    }

    // 简化时间文本赋值，减少冗余变量
    ui->time->setText(QTime(0, (currentPos / 60000) % 60, (currentPos / 1000) % 60).toString("mm:ss"));
}

// ========== 总时长更新（简化：删除冗余判断，简化时间赋值，保留原功能） ==========
void MyWidget::UpdateTotalDuration()
{
    qint64 totalDuration = mediaPlayer->duration() <= 0 ? 0 : mediaPlayer->duration();
    // 简化时间文本赋值，减少冗余变量
    ui->totaltime->setText(QTime(0, (totalDuration / 60000) % 60, (totalDuration / 1000) % 60).toString("mm:ss"));
    ui->times->setRange(0, 1000);
}

// ========== 工具函数：获取视频时长（保留原功能，简化返回值拼接） ==========
QString MyWidget::getMediaDuration(const QUrl& mediaUrl)
{
    QMediaPlayer tempPlayer;
    tempPlayer.setSource(mediaUrl);

    QEventLoop loop;
    connect(&tempPlayer, &QMediaPlayer::metaDataChanged, &loop, &QEventLoop::quit);
    QTimer::singleShot(800, &loop, &QEventLoop::quit);
    loop.exec();

    qint64 duration = tempPlayer.duration();
    if (duration <= 0) return "00:00";

    // 简化返回值拼接，减少冗余变量
    return QString("%1:%2").arg((duration / 60000) % 60, 2, 10, QChar('0'))
        .arg((duration / 1000) % 60, 2, 10, QChar('0'));
}

// ========== 日志写入（简化：删除冗余变量，简化文件操作，保留原功能） ==========
void MyWidget::logToFile(const QString &content)
{
    QtConcurrent::run([content]() {
        QFile file("player_log.txt");
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            // 简化文本流操作，减少冗余变量
            QTextStream stream(&file);
            stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                   << " - " << content << "\n";
        }
    });
}

// ========== 播放当前视频（简化：删除冗余的立即调用，保留原功能） ==========
void MyWidget::PlayCurrent()
{
    if (currentIndex < 0 || currentIndex >= sources.size()) return;

    videoItem->setGraphicsEffect(nullptr);
    mediaPlayer->setSource(sources[currentIndex]);
    mediaPlayer->play();
    ui->btStart->setText(tr("暂停"));
    playlistView->selectRow(currentIndex);
    change_action_state();

    // 延迟应用效果（保留原功能，删除冗余的立即调用，不影响效果）
    QTimer::singleShot(100, this, [this]() {
        updateVideoBrightness();
        updateVideoGeometry();
    });

    logToFile(QString("播放视频：") + sources[currentIndex].toLocalFile());
}

// ========== 以下函数：保留原功能，仅删除无意义的冗余注释，代码无改动 ==========
void MyWidget::TableClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    currentIndex = index.row();
    PlayCurrent();
}

void MyWidget::ClearSources()
{
    sources.clear();
    playlistModel->clear();
    currentIndex = -1;
    mediaPlayer->stop();
    ui->btStart->setText(tr("播放"));
    change_action_state();
    logToFile("清空播放列表");
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
        QRect screenGeometry = QApplication::primaryScreen()->availableGeometry();
        int x = screenGeometry.center().x() - playlistView->width() / 2;
        int y = screenGeometry.center().y() - playlistView->height() / 2;
        x = qMax(screenGeometry.left(), x);
        y = qMax(screenGeometry.top(), y);
        playlistView->move(x, y);
        playlistView->show();
        playlistView->raise();
        playlistView->activateWindow();
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

void MyWidget::showContextMenu(const QPoint &pos)
{
    mainMenu->popup(mapToGlobal(pos));
}

void MyWidget::aspectChanged(QAction *action)
{
    updateVideoGeometry();
}

void MyWidget::scaleChanged(QAction *action)
{
    updateVideoGeometry();
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

                MediaInfo info{url, fi.baseName(), duration};
                QMetaObject::invokeMethod(this, [this, info]() {
                    playlistModel->addMedia(info);
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

    // 1. 先判断：用户是否取消了文件选择（未选择保存路径）
    if (path.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("已取消保存播放记录"));
        return;
    }

    QFile file(path);
    // 2. 尝试打开文件，判断是否打开成功（保存的前提）
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        // 弹窗提示：保存失败（文件无法打开，可能是权限不足、路径非法等）
        QMessageBox::critical(this, tr("保存失败"), tr("无法创建/写入文件，请检查路径权限或文件是否被占用"));
        return;
    }

    // 3. 写入播放记录（原有核心逻辑，保持不变）
    QTextStream stream(&file);
    for (int i = 0; i < playlistModel->mediaCount(); ++i) {
        stream << playlistModel->mediaAt(i).url.toLocalFile() << "\n";
    }
    file.close(); // 手动关闭文件，确保数据写入完成

    // 4. 弹窗提示：保存成功
    QMessageBox::information(this, tr("保存成功"), tr("播放记录已成功保存到：\n%1").arg(path));

    // 原有日志写入逻辑，保留不变
    logToFile(QString("导出播放列表：") + path);
}

void MyWidget::toggleFullScreen(bool checked)
{
    if (checked) {
        this->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        this->showFullScreen();
        isFullScreenMode = true;
    } else {
        this->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
        this->showNormal();
        isFullScreenMode = false;
    }

    updateVideoGeometry();
    this->layout()->update();
    this->adjustSize();
}

// ========== 按钮槽函数（保留原功能，无冗余改动） ==========
void MyWidget::on_btLast_clicked()
{
    if (sources.isEmpty() || currentIndex <= 0) return;
    currentIndex--;
    PlayCurrent();
}

void MyWidget::on_btNext_clicked()
{
    if (sources.isEmpty() || currentIndex >= static_cast<int>(sources.size()) - 1) return;
    currentIndex++;
    PlayCurrent();
}

void MyWidget::on_btStart_clicked()
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

void MyWidget::on_btReset_clicked()
{
    mediaPlayer->stop();
    ui->btStart->setText(tr("播放"));
    updateVideoBrightness();
}

void MyWidget::on_btUpload_clicked()
{
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

            MediaInfo info{url, fi.baseName(), duration};
            QMetaObject::invokeMethod(this, [this, info]() {
                playlistModel->addMedia(info);
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
    if (playlistView->isHidden()) {
        QRect screenRect = QApplication::primaryScreen()->availableGeometry();
        QSize listSize = playlistView->size();
        int x = screenRect.center().x() - listSize.width() / 2;
        int y = screenRect.center().y() - listSize.height() / 2;
        x = qMax(screenRect.left(), qMin(x, screenRect.right() - listSize.width()));
        y = qMax(screenRect.top(), qMin(y, screenRect.bottom() - listSize.height()));
        playlistView->move(x, y);
        playlistView->show();
        playlistView->raise();
        playlistView->activateWindow();
    } else {
        playlistView->hide();
    }
}

void MyWidget::on_btExport_clicked()
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
        stream << playlistModel->mediaAt(i).url.toLocalFile() << "\n";
    }
    file.close();
    logToFile(QString("导出播放列表：") + path);
}

void MyWidget::on_btClose_clicked()
{
    qApp->quit();
}

void MyWidget::on_slider_valueChanged(int value)
{
    double volume = static_cast<double>(value) / 100.0;
    audioOutput->setVolume(volume);
}

void MyWidget::on_times_valueChanged(int value)
{
    if (!mediaPlayer || mediaPlayer->duration() <= 0) return;

    qint64 targetPos = static_cast<qint64>((static_cast<qint64>(value) * mediaPlayer->duration()) / 1000);
    mediaPlayer->setPosition(targetPos);

    QTime currentTime(0, (targetPos / 60000) % 60, (targetPos / 1000) % 60);
    ui->time->setText(currentTime.toString("mm:ss"));
}

void MyWidget::on_lights_valueChanged(int value)
{
    updateVideoBrightness();
}

void MyWidget::closeEvent(QCloseEvent *event)
{
    hide();
    tray_icon->showMessage(tr("士麦那视频播放器"), tr("单击我重新回到主界面"), QSystemTrayIcon::Information, 2000);
    event->ignore();
}

void MyWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateVideoGeometry();
}

void MyWidget::viewResized(const QRect &rect)
{
    Q_UNUSED(rect);
    updateVideoGeometry();
}
