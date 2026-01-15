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
#include <QTimer>
#include <QGraphicsScene>
#include <QGraphicsColorizeEffect>

// 全局信号量：限制同时获取时长的线程数
static QSemaphore durationFetchSemaphore(2);

// 构造函数实现
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
    , currentBrightness(0.0)
    , slider_brightness(nullptr)
    , currentColor(Qt::white)
    , playlistModel(new PlaylistModel(this))
    , playlistView(new QTableView(this))
    , isFullScreenMode(false)
    , progressUpdateTimer(new QTimer(this))
{
    ui->setupUi(this);

    setWindowTitle(tr("士麦那视频播放器"));

    // ========== 核心优化：消除所有页面空白，界面完全占满 ==========
    // 1. 主窗口布局无内边距、无间距
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (mainLayout) {
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
    }

    // 2. 使用 QGraphicsView 替代 QVideoWidget
    videoView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoView->setStyleSheet("background-color: black; border: none;");
    videoView->setContentsMargins(0, 0, 0, 0);
    videoView->setMinimumSize(0, 0);
    videoView->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    videoView->setRenderHint(QPainter::Antialiasing, false);
    videoView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    videoView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    videoView->setFrameShape(QFrame::NoFrame);
    videoView->setAlignment(Qt::AlignCenter);

    // 创建场景并添加视频项
    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->addItem(videoItem);
    videoView->setScene(scene);

    // 3. 用 ui->widget1 承载 videoView
    QVBoxLayout *videoLayout = new QVBoxLayout(ui->widget1);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(0);
    videoLayout->addWidget(videoView);
    videoLayout->setStretchFactor(videoView, 1);

    ui->widget1->setLayout(videoLayout);
    ui->widget1->setContentsMargins(0, 0, 0, 0);
    ui->widget1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->widget1->setMinimumSize(0, 0);
    ui->widget1->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    // 4. 下方控件布局优化，减少冗余空白
    QVBoxLayout *bottomLayout = qobject_cast<QVBoxLayout*>(ui->verticalLayout);
    if (bottomLayout) {
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setSpacing(2);
    }
    ui->verticalLayout->setContentsMargins(0, 0, 0, 0);

    // 5. 设置拉伸比例
    if (mainLayout) {
        mainLayout->setStretchFactor(ui->widget1, 99);
        mainLayout->setStretchFactor(ui->verticalLayout, 1);
    }

    // ========== 播放器初始化 ==========
    mediaPlayer->setVideoOutput(videoItem);
    mediaPlayer->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.5);

    // ========== 播放进度滑块初始化 ==========
    QSlider *seek_slider = ui->times;
    seek_slider->setRange(0, 1000);
    seek_slider->setContentsMargins(0, 0, 0, 0);

    // ========== 亮度滑块初始化 ==========
    slider_brightness = ui->lights;
    slider_brightness->setRange(-100, 100);
    slider_brightness->setValue(0);
    slider_brightness->setContentsMargins(0, 0, 0, 0);

    // ========== 播放列表：白底黑字设置 ==========
    playlistView->setStyleSheet(R"(
        QTableView {
            background-color: white;
            color: black;
            border: 1px solid #cccccc;
            gridline-color: #eeeeee;
        }
        QTableView::header {
            background-color: #f5f5f5;
            color: black;
        }
        QTableView::item:selected {
            background-color: #add8e6;
            color: black;
        }
    )");
    QPalette playlistPalette = playlistView->palette();
    playlistPalette.setColor(QPalette::Base, Qt::white);
    playlistPalette.setColor(QPalette::Text, Qt::black);
    playlistPalette.setColor(QPalette::Highlight, QColor(173, 216, 230));
    playlistPalette.setColor(QPalette::HighlightedText, Qt::black);
    playlistView->setPalette(playlistPalette);
    playlistView->setAttribute(Qt::WA_OpaquePaintEvent, true);

    // ========== 播放列表初始化 ==========
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
    // 连接 clicked 信号到现有 TableClicked 函数
    connect(playlistView, &QTableView::clicked, this, &MyWidget::TableClicked);

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
    videoView->setContextMenuPolicy(Qt::CustomContextMenu);
    createContextMenu();

    // ========== 进度滑块自动跟随 + 总时长更新 ==========
    progressUpdateTimer->setInterval(50);
    connect(progressUpdateTimer, &QTimer::timeout, this, &MyWidget::UpdatePlaybackProgress);
    connect(mediaPlayer, &QMediaPlayer::durationChanged, this, &MyWidget::UpdateTotalDuration);
    connect(mediaPlayer, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::PlayingState) {
            progressUpdateTimer->start();
        } else {
            progressUpdateTimer->stop();
        }
    });

    // ========== 按钮初始状态 ==========
    ui->btStart->setEnabled(false);
    ui->btReset->setEnabled(false);
    ui->btLast->setEnabled(false);
    ui->btNext->setEnabled(false);

    // 初始设置亮度
    updateVideoBrightness();
}

// 析构函数
MyWidget::~MyWidget()
{
    delete ui;
}

// ========== 亮度调节 - 使用 QGraphicsColorizeEffect ==========
void MyWidget::updateVideoBrightness()
{
    int value = slider_brightness->value();

    // 移除已有的效果
    videoItem->setGraphicsEffect(nullptr);

    if (value != 0) {
        // 创建颜色化效果
        QGraphicsColorizeEffect *colorizeEffect = new QGraphicsColorizeEffect(this);

        if (value < 0) {
            // 变暗：使用黑色，强度根据滑块值计算
            colorizeEffect->setColor(Qt::black);
            // 将 -100~0 映射到 0~1 的强度
            float strength = qAbs(value) / 100.0f;
            colorizeEffect->setStrength(strength * 0.8f); // 限制最大强度为0.8，避免完全变黑
        } else {
            // 变亮：使用白色，需要不同的方法
            // 使用透明度叠加的方法来模拟变亮
            QGraphicsOpacityEffect *opacityEffect = new QGraphicsOpacityEffect(this);

            // 创建一个小部件用于白色覆盖
            // 注意：由于我们使用 QGraphicsVideoItem，需要特殊处理
            // 这里采用调整视频亮度属性的方法

            // 对于变亮，我们可以调整视频的亮度/对比度
            // 但由于 Qt 没有直接API，使用颜色矩阵变换
            QGraphicsColorizeEffect *brightnessEffect = new QGraphicsColorizeEffect(this);
            brightnessEffect->setColor(Qt::white);

            // 将 0~100 映射到 0~0.5 的强度（避免完全变白）
            float brightnessStrength = value / 200.0f;
            brightnessEffect->setStrength(brightnessStrength);

            videoItem->setGraphicsEffect(brightnessEffect);
            return;
        }

        videoItem->setGraphicsEffect(colorizeEffect);
    }
}

// ========== 更新视频尺寸 ==========
void MyWidget::updateVideoGeometry()
{
    if (!videoItem || !videoView) return;

    // 获取视频原始尺寸
    QSizeF videoSize = videoItem->nativeSize();
    if (videoSize.isEmpty()) {
        videoSize = QSizeF(640, 480); // 默认尺寸
    }

    // 获取视图尺寸
    QRectF viewRect = videoView->sceneRect();
    if (viewRect.isEmpty()) {
        viewRect = videoView->rect();
    }

    // 计算保持宽高比的最佳尺寸
    qreal videoAspect = videoSize.width() / videoSize.height();
    qreal viewAspect = viewRect.width() / viewRect.height();

    if (videoAspect > viewAspect) {
        // 视频更宽，按宽度缩放
        qreal newHeight = viewRect.width() / videoAspect;
        videoItem->setSize(QSizeF(viewRect.width(), newHeight));
    } else {
        // 视频更高，按高度缩放
        qreal newWidth = viewRect.height() * videoAspect;
        videoItem->setSize(QSizeF(newWidth, viewRect.height()));
    }

    // 居中显示
    videoItem->setPos((viewRect.width() - videoItem->size().width()) / 2,
                      (viewRect.height() - videoItem->size().height()) / 2);
}

// ========== 进度实时更新 ==========
void MyWidget::UpdatePlaybackProgress()
{
    if (!mediaPlayer || mediaPlayer->duration() <= 0) return;

    qint64 currentPos = mediaPlayer->position();
    qint64 totalDuration = mediaPlayer->duration();
    int sliderValue = static_cast<int>((static_cast<qint64>(1000) * currentPos) / totalDuration);

    if (ui->times->value() != sliderValue) {
        ui->times->blockSignals(true);
        ui->times->setValue(sliderValue);
        ui->times->blockSignals(false);
    }

    QTime currentTime(0, (currentPos / 60000) % 60, (currentPos / 1000) % 60);
    ui->time->setText(currentTime.toString("mm:ss"));
}

// ========== 总时长更新 ==========
void MyWidget::UpdateTotalDuration()
{
    if (!mediaPlayer || mediaPlayer->duration() <= 0) {
        ui->totaltime->setText("00:00");
        return;
    }

    qint64 totalDuration = mediaPlayer->duration();
    QTime totalTime(0, (totalDuration / 60000) % 60, (totalDuration / 1000) % 60);
    ui->totaltime->setText(totalTime.toString("mm:ss"));

    ui->times->setRange(0, 1000);
}

// ========== 工具函数 ==========
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

    // 先移除效果，播放时重新应用
    videoItem->setGraphicsEffect(nullptr);

    mediaPlayer->setSource(sources[currentIndex]);
    mediaPlayer->play();
    ui->btStart->setText(tr("暂停"));
    playlistView->selectRow(currentIndex);
    change_action_state();

    // 延迟应用亮度效果，确保视频已加载
    QTimer::singleShot(100, this, &MyWidget::updateVideoBrightness);

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
    playlistModel->clear();
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
    connect(fullScreenAction, &QAction::toggled, this, &MyWidget::toggleFullScreen);

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
    mainMenu->popup(mapToGlobal(pos));
}

void MyWidget::aspectChanged(QAction *action)
{
    // 保持宽高比设置
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

// ========== 按钮槽函数 ==========
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
    updateVideoBrightness(); // 重置时更新亮度
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

// ========== 亮度滑块变化 ==========
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
