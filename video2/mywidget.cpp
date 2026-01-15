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
#include <QGraphicsOpacityEffect>
#include <QPainter>

// 全局信号量：限制同时获取时长的线程数
static QSemaphore durationFetchSemaphore(2);

// 构造函数实现
MyWidget::MyWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MyWidget)
    , mediaPlayer(new QMediaPlayer(this))
    , videoWidget(new QVideoWidget(this))
    , audioOutput(new QAudioOutput(this))
    , currentIndex(-1)
    , tray_icon(nullptr)
    , mainMenu(nullptr)
    , currentBrightness(1.0)
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

    // 2. videoWidget 拉伸填充，无背景空白
    videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoWidget->setStyleSheet("background-color: black;");
    videoWidget->setContentsMargins(0, 0, 0, 0);
    videoWidget->setMinimumSize(0, 0);
    videoWidget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    // 3. 用 ui->widget1 承载 videoWidget
    QVBoxLayout *videoLayout = new QVBoxLayout(ui->widget1);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(0);
    videoLayout->addWidget(videoWidget);
    videoLayout->setStretchFactor(videoWidget, 1);

    // ========== 亮度叠加层 - 关键修复 ==========
    brightnessOverlay = new QWidget(videoWidget);
    brightnessOverlay->setObjectName("brightnessOverlay");

    // 关键设置：确保覆盖层能正确显示
    brightnessOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true); // 不接收鼠标事件
    brightnessOverlay->setAttribute(Qt::WA_NoSystemBackground, true); // 不绘制系统背景
    brightnessOverlay->setAttribute(Qt::WA_OpaquePaintEvent, false); // 允许透明
    brightnessOverlay->setAutoFillBackground(false);

    // 初始设置为完全透明
    brightnessOverlay->setStyleSheet("background-color: rgba(0,0,0,0);");

    // 确保叠加层始终在视频之上
    brightnessOverlay->raise();
    brightnessOverlay->show();

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
    mediaPlayer->setVideoOutput(videoWidget);
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
    // 关键：连接 clicked 信号到现有 TableClicked 函数，实现点击切换视频
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
    videoWidget->setContextMenuPolicy(Qt::CustomContextMenu);
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
}

// 析构函数
MyWidget::~MyWidget()
{
    delete ui;
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

    updateBrightnessOverlay();

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

// ========== 亮度调节 - 修复版本 ==========
void MyWidget::on_lights_valueChanged(int value)
{
    updateBrightnessOverlay();
}

void MyWidget::updateBrightnessOverlay()
{
    if (!brightnessOverlay || !videoWidget) return;

    int value = slider_brightness->value();

    // 确保亮度叠加层始终在视频之上
    brightnessOverlay->setParent(videoWidget);
    brightnessOverlay->setGeometry(videoWidget->rect());
    brightnessOverlay->raise();
    brightnessOverlay->show();

    // 计算透明度（0-255）
    int alpha = qAbs(value) * 2;
    if (alpha > 255) alpha = 255;

    // 设置颜色
    QString styleSheet;
    if (value == 0) {
        styleSheet = "background-color: rgba(0,0,0,0);";
    } else if (value < 0) {
        // 变暗：黑色半透明层
        styleSheet = QString("background-color: rgba(0, 0, 0, %1);").arg(alpha);
    } else {
        // 变亮：白色半透明层
        styleSheet = QString("background-color: rgba(255, 255, 255, %1);").arg(alpha);
    }

    // 应用样式并强制更新
    brightnessOverlay->setStyleSheet(styleSheet);
    brightnessOverlay->update();
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
    updateBrightnessOverlay();
}

bool MyWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == videoWidget && event->type() == QEvent::Resize) {
        updateBrightnessOverlay();
    }
    return QWidget::eventFilter(watched, event);
}
