#include "mywidget.h"
#include "ui_mywidget.h"
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
#include <QSemaphore> // 新增：用于限制并发

// 全局信号量：限制同时获取时长的线程数
static QSemaphore durationFetchSemaphore(2);

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

{
    ui->setupUi(this);

    setWindowTitle(tr("士麦那视频播放器"));
    setWindowIcon(QIcon(":/images/icon.png"));

    // ========== 视频区域 + 亮度叠加层 ==========
    QVBoxLayout *videoLayout = new QVBoxLayout(ui->tableWidget);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->addWidget(videoWidget);
    videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 创建亮度叠加层（黑色/白色半透明）
    brightnessOverlay = new QWidget(videoWidget);
    brightnessOverlay->setGeometry(videoWidget->rect());
    brightnessOverlay->setAttribute(Qt::WA_TransparentForMouseEvents); // 鼠标穿透
    brightnessOverlay->hide(); // 初始隐藏

    // 播放器初始化
    mediaPlayer->setVideoOutput(videoWidget);
    mediaPlayer->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.5);

    // ========== 播放进度滑块 ==========
    QSlider *seek_slider = ui->times;
    seek_slider->setRange(0, 1000);
    connect(seek_slider, &QSlider::valueChanged, this, [=](int value) {
        if (mediaPlayer->duration() > 0) {
            mediaPlayer->setPosition(static_cast<qint64>(value * mediaPlayer->duration() / 1000.0));
        }
    });
    connect(mediaPlayer, &QMediaPlayer::positionChanged, this, [=](qint64 pos) {
        if (mediaPlayer->duration() > 0) {
            seek_slider->setValue(static_cast<int>(pos * 1000.0 / mediaPlayer->duration()));
        } else {
            seek_slider->setValue(0);
        }
    });

    // ========== 亮度滑块（新方案） ==========
    slider_brightness = ui->lights;
    slider_brightness->setRange(-100, 100);
    slider_brightness->setValue(0);
    connect(slider_brightness, &QSlider::valueChanged, this, [=](int value) {
        if (value == 0) {
            brightnessOverlay->hide();
        } else if (value < 0) {
            // 变暗：叠加黑色
            int alpha = (-value) * 2.55; // 0～255
            brightnessOverlay->setStyleSheet(QString("background-color: rgba(0,0,0,%1);").arg(alpha));
            brightnessOverlay->show();
        } else {
            // 变亮：叠加白色
            int alpha = value * 2.55;
            brightnessOverlay->setStyleSheet(QString("background-color: rgba(255,255,255,%1);").arg(alpha));
            brightnessOverlay->show();
        }
    });

    // ========== 播放列表 ==========
    playlistView->setModel(playlistModel);
    playlistView->setColumnWidth(0, 200);
    playlistView->setWindowTitle(tr("播放列表"));
    playlistView->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    playlistView->resize(400, 400);
    playlistView->setSelectionMode(QAbstractItemView::SingleSelection);
    playlistView->setSelectionBehavior(QAbstractItemView::SelectRows);
    playlistView->setShowGrid(false);
    connect(playlistView, &QTableView::clicked, this, &MyWidget::TableClicked);
    playlistView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(playlistView, &QWidget::customContextMenuRequested, this, [=](const QPoint &pos) {
        QMenu menu(playlistView);
        menu.addAction(tr("清空列表"), this, &MyWidget::ClearSources);
        menu.exec(playlistView->mapToGlobal(pos));
    });

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
    connect(tray_icon, &QSystemTrayIcon::activated, this, &MyWidget::TrayIconActivated);
    tray_icon->show();

    // ========== 上下文菜单 ==========
    setContextMenuPolicy(Qt::CustomContextMenu);
    videoWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(videoWidget, &QWidget::customContextMenuRequested, this, &MyWidget::showContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &MyWidget::showContextMenu);
    createContextMenu();

    // ========== 播放时长显示 ==========
    connect(mediaPlayer, &QMediaPlayer::positionChanged, this, &MyWidget::UpdateTime);
    connect(mediaPlayer, &QMediaPlayer::durationChanged, this, &MyWidget::UpdateTime);

    // ========== 按钮初始状态 ==========
    ui->btStart->setEnabled(false);
    ui->btReset->setEnabled(false);
    ui->btLast->setEnabled(false);
    ui->btNext->setEnabled(false);

    // ========== 新增按钮连接 ==========
    connect(ui->btExport, &QPushButton::clicked, this, &MyWidget::exportPlaylist);
    connect(ui->btClose, &QPushButton::clicked, qApp, &QApplication::quit);
}

MyWidget::～MyWidget()
{
    delete ui;
}

// ========== 获取媒体文件时长（带并发限制） ==========
QString MyWidget::getMediaDuration(const QUrl& mediaUrl)
{
    QMediaPlayer tempPlayer;
    tempPlayer.setSource(mediaUrl);

    QEventLoop loop;
    QObject::connect(&tempPlayer, &QMediaPlayer::metaDataChanged, &loop, &QEventLoop::quit);
    QTimer::singleShot(800, &loop, &QEventLoop::quit); // 延长一点
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

void MyWidget::on_slider_valueChanged(int value)
{
    double volume = static_cast<double>(value) / 100.0;
    audioOutput->setVolume(volume);
}

// ========== 统一播放当前项 ==========
void MyWidget::PlayCurrent()
{
    if (currentIndex < 0 || currentIndex >= sources.size()) return;

    mediaPlayer->setSource(sources[currentIndex]);
    mediaPlayer->play();
    ui->btStart->setText(tr("暂停"));
    playlistView->selectRow(currentIndex);
    change_action_state();
    logToFile("播放视频：" + sources[currentIndex].toLocalFile());
}

void MyWidget::TableClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    currentIndex = index.row();
    PlayCurrent();
}

void MyWidget::OpenFile()
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
            durationFetchSemaphore.acquire(); // 限流
            QString duration = getMediaDuration(url);
            durationFetchSemaphore.release();

            MediaInfo info{url, fi.baseName(), duration};
            QMetaObject::invokeMethod(this, [this, info]() {
                playlistModel->addMedia(info);
            });
        });
        logToFile("打开文件：" + f);
    }

    if (oldCount == 0 && !sources.isEmpty()) {
        currentIndex = 0;
        PlayCurrent();
    }
    change_action_state();
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
    logToFile("导入播放列表：" + path);
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
    logToFile("导出播放列表：" + path);
}

void MyWidget::createContextMenu()
{
    mainMenu = new QMenu(this);

    QMenu *aspectMenu = mainMenu->addMenu(tr("宽高比"));
    QActionGroup *aspectGroup = new QActionGroup(aspectMenu);
    connect(aspectGroup, &QActionGroup::triggered, this, &MyWidget::aspectChanged);
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
    connect(scaleGroup, &QActionGroup::triggered, this, &MyWidget::scaleChanged);
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
    connect(fullScreenAction, &QAction::toggled, this, [=](bool checked) {
        checked ? videoWidget->showFullScreen() : videoWidget->showNormal();
    });

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

void MyWidget::UpdateTime(qint64)
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

void MyWidget::closeEvent(QCloseEvent *event)
{
    hide();
    tray_icon->showMessage(tr("士麦那视频播放器"), tr("单击我重新回到主界面"), QSystemTrayIcon::Information, 2000);
    event->ignore();
}

// ========== 按钮槽函数 ==========
void MyWidget::on_btLast_clicked() { SkipBackward(); }
void MyWidget::on_btNext_clicked() { SkipForward(); }
void MyWidget::on_btStart_clicked() { SetPaused(); }
void MyWidget::on_btReset_clicked() {
    mediaPlayer->stop();
    ui->btStart->setText(tr("播放"));
}
void MyWidget::on_btUpload_clicked() { OpenFile(); }
void MyWidget::on_btList_clicked() { SetPlayListShown(); }
