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
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>

// 全局信号量：限制同时获取视频时长的并发线程数
static QSemaphore durationFetchSemaphore(2);

// 构造函数：初始化播放器所有核心组件与布局
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
    , slider_brightness(nullptr)
    , playlistModel(new PlaylistModel(this))
    , playlistView(new QTableView(this))
    , isFullScreenMode(false)
    , progressUpdateTimer(new QTimer(this))
{
    ui->setupUi(this);
    setWindowTitle(tr("士麦那视频播放器"));

    initLayoutSetting();
    initVideoPlayer();
    initPlaylistView();
    initTrayAndMenu();
    initSignalConnection();

    ui->btStart->setEnabled(false);
    ui->btReset->setEnabled(false);
    ui->btLast->setEnabled(false);
    ui->btNext->setEnabled(false);

    updateVideoBrightness();
}

// 析构函数：释放UI资源
MyWidget::~MyWidget()
{
    delete ui;
}

// 初始化播放器整体布局与视频显示区域样式
void MyWidget::initLayoutSetting()
{
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (mainLayout) {
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        mainLayout->setStretchFactor(ui->widget1, 99);
        mainLayout->setStretchFactor(ui->verticalLayout, 1);
    }

    videoView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoView->setStyleSheet("background-color: black; border: none;");
    videoView->setContentsMargins(0, 0, 0, 0);
    videoView->setRenderHint(QPainter::Antialiasing, false);
    videoView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    videoView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    videoView->setFrameShape(QFrame::NoFrame);
    videoView->setAlignment(Qt::AlignCenter);

    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->addItem(videoItem);
    videoView->setScene(scene);

    QVBoxLayout *videoLayout = new QVBoxLayout(ui->widget1);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(0);
    videoLayout->addWidget(videoView);
    videoLayout->setStretchFactor(videoView, 1);

    QVBoxLayout *bottomLayout = qobject_cast<QVBoxLayout*>(ui->verticalLayout);
    if (bottomLayout) {
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setSpacing(2);
    }
}

// 初始化媒体播放器、音量与进度/亮度滑块参数
void MyWidget::initVideoPlayer()
{
    mediaPlayer->setVideoOutput(videoItem);
    mediaPlayer->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.5);

    ui->times->setRange(0, 1000);
    ui->times->setContentsMargins(0, 0, 0, 0);

    slider_brightness = ui->lights;
    slider_brightness->setRange(-100, 100);
    slider_brightness->setValue(0);
    slider_brightness->setContentsMargins(0, 0, 0, 0);

    progressUpdateTimer->setInterval(50);
}

// 初始化播放列表视图样式与数据模型绑定
void MyWidget::initPlaylistView()
{
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

// 初始化系统托盘与右键上下文菜单
void MyWidget::initTrayAndMenu()
{
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

    mainMenu = new QMenu(this);
    setContextMenuPolicy(Qt::CustomContextMenu);
    videoView->setContextMenuPolicy(Qt::CustomContextMenu);

    QMenu *aspectMenu = mainMenu->addMenu(tr("宽高比"));
    QActionGroup *aspectGroup = new QActionGroup(aspectMenu);
    aspectGroup->setExclusive(true);
    addCheckableAction(aspectMenu, aspectGroup, tr("自动"), true);
    addCheckableAction(aspectMenu, aspectGroup, tr("16:9"),false);

    QMenu *scaleMenu = mainMenu->addMenu(tr("缩放模式"));
    QActionGroup *scaleGroup = new QActionGroup(scaleMenu);
    scaleGroup->setExclusive(true);
    addCheckableAction(scaleMenu, scaleGroup, tr("不缩放"), true);
    addCheckableAction(scaleMenu, scaleGroup, tr("1.2倍缩放"),false);

    QAction *fullScreenAction = mainMenu->addAction(tr("全屏"));
    fullScreenAction->setCheckable(true);
    connect(fullScreenAction, &QAction::toggled, this, &MyWidget::toggleFullScreen);

    mainMenu->addSeparator();
    mainMenu->addAction(tr("导入播放列表"), this, &MyWidget::importPlaylist);
    mainMenu->addAction(tr("导出播放列表"), this, &MyWidget::exportPlaylist);
}

// 快速创建带选中状态的菜单动作并加入动作组
void MyWidget::addCheckableAction(QMenu *menu, QActionGroup *group, const QString &text, bool checked)
{
    QAction *act = menu->addAction(text);
    act->setCheckable(true);
    act->setChecked(checked);
    group->addAction(act);
}

// 统一绑定播放器所有核心信号与槽函数
void MyWidget::initSignalConnection()
{
    connect(playlistView, &QTableView::clicked, this, &MyWidget::TableClicked);

    connect(progressUpdateTimer, &QTimer::timeout, this, &MyWidget::UpdatePlaybackProgress);
    connect(mediaPlayer, &QMediaPlayer::durationChanged, this, &MyWidget::UpdateTotalDuration);
    connect(mediaPlayer, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::PlayingState) {
            progressUpdateTimer->start();
        } else {
            progressUpdateTimer->stop();
        }
    });

    connect(tray_icon, &QSystemTrayIcon::activated, this, &MyWidget::TrayIconActivated);
}

// 调整视频播放区域的亮度效果
void MyWidget::updateVideoBrightness()
{
    int value = slider_brightness->value();
    videoItem->setGraphicsEffect(nullptr);

    if (value != 0) {
        QGraphicsColorizeEffect *effect = new QGraphicsColorizeEffect(this);
        effect->setColor(value < 0 ? Qt::black : Qt::white);
        float strength = qAbs(value) / 100.0f;
        effect->setStrength(value < 0 ? strength * 0.8f : strength * 0.5f);

        videoItem->setGraphicsEffect(effect);
    }
}

// 根据窗口尺寸更新视频的显示尺寸与居中位置
void MyWidget::updateVideoGeometry()
{
    if (!videoItem || !videoView || !videoView->scene()) return;

    QSizeF videoSize = videoItem->nativeSize().isEmpty() ? QSizeF(640, 480) : videoItem->nativeSize();
    QRectF viewRect = videoView->viewport()->rect();
    if (viewRect.isEmpty()) return;

    qreal videoAspect = videoSize.width() / videoSize.height();
    qreal viewAspect = viewRect.width() / viewRect.height();
    QSizeF targetSize = videoAspect > viewAspect
                            ? QSizeF(viewRect.width(), viewRect.width() / videoAspect)
                            : QSizeF(viewRect.height() * videoAspect, viewRect.height());

    videoItem->setSize(targetSize);
    videoItem->setPos(viewRect.center() - videoItem->boundingRect().center());
    videoView->update();
}

// 定时更新播放进度条与当前播放时间显示
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

    ui->time->setText(QTime(0, (currentPos / 60000) % 60, (currentPos / 1000) % 60).toString("mm:ss"));
}

// 更新视频总时长显示与进度条范围
void MyWidget::UpdateTotalDuration()
{
    qint64 totalDuration = mediaPlayer->duration() <= 0 ? 0 : mediaPlayer->duration();
    ui->totaltime->setText(QTime(0, (totalDuration / 60000) % 60, (totalDuration / 1000) % 60).toString("mm:ss"));
    ui->times->setRange(0, 1000);
}

// 获取指定视频文件的时长并格式化返回
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

    return QString("%1:%2").arg((duration / 60000) % 60, 2, 10, QChar('0'))
        .arg((duration / 1000) % 60, 2, 10, QChar('0'));
}

// 异步写入操作日志到本地player_log.txt文件
void MyWidget::logToFile(const QString &content)
{
    QtConcurrent::run([content]() {
        QFile file("player_log.txt");
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                   << " - " << content << "\n";
        }
    });
}

// 加载并播放当前选中索引对应的视频文件
void MyWidget::PlayCurrent()
{
    if (currentIndex < 0 || currentIndex >= sources.size()) return;

    videoItem->setGraphicsEffect(nullptr);
    mediaPlayer->setSource(sources[currentIndex]);
    mediaPlayer->play();
    ui->btStart->setText(tr("暂停"));
    playlistView->selectRow(currentIndex);
    change_action_state();

    QTimer::singleShot(100, this, [this]() {
        updateVideoBrightness();
        updateVideoGeometry();
    });

    logToFile(QString("播放视频：") + sources[currentIndex].toLocalFile());
}

// 响应播放列表点击事件，切换播放对应的视频
void MyWidget::TableClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    currentIndex = index.row();
    PlayCurrent();
}

// 清空播放列表与视频源数据，重置播放状态
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

// 切换视频的播放/暂停状态
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

// 播放列表中切换到上一个视频文件
void MyWidget::SkipBackward()
{
    if (sources.isEmpty() || currentIndex <= 0) return;
    currentIndex--;
    PlayCurrent();
}

// 播放列表中切换到下一个视频文件
void MyWidget::SkipForward()
{
    if (sources.isEmpty() || currentIndex >= static_cast<int>(sources.size()) - 1) return;
    currentIndex++;
    PlayCurrent();
}

// 显示/隐藏播放列表窗口
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

// 更新播放器控制按钮的可用状态
void MyWidget::change_action_state()
{
    bool hasMedia = !sources.isEmpty();
    ui->btStart->setEnabled(hasMedia);
    ui->btReset->setEnabled(hasMedia);
    ui->btLast->setEnabled(hasMedia && currentIndex > 0);
    ui->btNext->setEnabled(hasMedia && currentIndex < static_cast<int>(sources.size()) - 1);
}

// 显示右键上下文菜单
void MyWidget::showContextMenu(const QPoint &pos)
{
    mainMenu->popup(mapToGlobal(pos));
}

// 响应宽高比切换事件，更新视频显示布局
void MyWidget::aspectChanged(QAction *action)
{
    updateVideoGeometry();
}

// 响应缩放模式切换事件，更新视频显示布局
void MyWidget::scaleChanged(QAction *action)
{
    updateVideoGeometry();
}

// 响应系统托盘点击事件，恢复播放器窗口显示
void MyWidget::TrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        showNormal();
        activateWindow();
    }
}

// 从本地文本文件导入播放列表
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

// 导出当前播放列表到本地文本文件，带自定义弹窗提示
void MyWidget::exportPlaylist()
{
    QString path = QFileDialog::getSaveFileName(this, tr("导出播放列表"), "", "文本文件 (*.txt)");

    if (path.isEmpty()) {
        showCustomDialog(tr("提示"), tr("已取消保存播放记录"), Qt::black);
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showCustomDialog(tr("保存失败"), tr("无法创建/写入文件，请检查路径权限或文件是否被占用"), Qt::red);
        return;
    }

    QTextStream stream(&file);
    for (int i = 0; i < playlistModel->mediaCount(); ++i) {
        stream << playlistModel->mediaAt(i).url.toLocalFile() << "\n";
    }
    file.close();

    QString successContent = tr("播放记录已成功保存到：\n%1").arg(path);
    showCustomDialog(tr("保存成功"), successContent, Qt::green);

    logToFile(QString("导出播放列表：") + path);
}

// 切换播放器的全屏/普通窗口显示模式
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

// 响应上一个按钮点击，切换到上一个视频
void MyWidget::on_btLast_clicked()
{
    if (sources.isEmpty() || currentIndex <= 0) return;
    currentIndex--;
    PlayCurrent();
}

// 响应下一个按钮点击，切换到下一个视频
void MyWidget::on_btNext_clicked()
{
    if (sources.isEmpty() || currentIndex >= static_cast<int>(sources.size()) - 1) return;
    currentIndex++;
    PlayCurrent();
}

// 响应播放/暂停按钮点击，切换播放状态
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

// 响应重置按钮点击，停止视频播放并重置状态
void MyWidget::on_btReset_clicked()
{
    mediaPlayer->stop();
    ui->btStart->setText(tr("播放"));
    updateVideoBrightness();
}

// 响应打开文件按钮点击，选择并添加视频文件到播放列表
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

// 响应播放列表按钮点击，显示/隐藏播放列表窗口
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

// 响应导出按钮点击，调用播放列表导出功能
void MyWidget::on_btExport_clicked()
{
    exportPlaylist();
}

// 响应关闭按钮点击，退出应用程序
void MyWidget::on_btClose_clicked()
{
    qApp->quit();
}

// 响应音量滑块变动，调整视频播放音量
void MyWidget::on_slider_valueChanged(int value)
{
    double volume = static_cast<double>(value) / 100.0;
    audioOutput->setVolume(volume);
}

// 响应进度条拖动，调整视频播放位置
void MyWidget::on_times_valueChanged(int value)
{
    if (!mediaPlayer || mediaPlayer->duration() <= 0) return;

    qint64 targetPos = static_cast<qint64>((static_cast<qint64>(value) * mediaPlayer->duration()) / 1000);
    mediaPlayer->setPosition(targetPos);

    QTime currentTime(0, (targetPos / 60000) % 60, (targetPos / 1000) % 60);
    ui->time->setText(currentTime.toString("mm:ss"));
}

// 响应亮度滑块变动，更新视频亮度
void MyWidget::on_lights_valueChanged(int value)
{
    updateVideoBrightness();
}

// 响应窗口关闭事件，隐藏窗口到系统托盘
void MyWidget::closeEvent(QCloseEvent *event)
{
    hide();
    tray_icon->showMessage(tr("士麦那视频播放器"), tr("单击我重新回到主界面"), QSystemTrayIcon::Information, 2000);
    event->ignore();
}

// 响应窗口尺寸变动事件，更新视频显示布局
void MyWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateVideoGeometry();
}

// 响应视图尺寸变动，更新视频显示布局
void MyWidget::viewResized(const QRect &rect)
{
    Q_UNUSED(rect);
    updateVideoGeometry();
}

// 自定义弹窗工具函数：显示白底指定文字颜色的提示弹窗
int MyWidget::showCustomDialog(const QString &title, const QString &content, const QColor &textColor)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setFixedSize(320, 160);
    dialog.setStyleSheet("background-color: white; border: 1px solid #eeeeee;");

    QLabel *contentLabel = new QLabel(&dialog);
    contentLabel->setText(content);
    contentLabel->setWordWrap(true);
    contentLabel->setAlignment(Qt::AlignCenter);
    contentLabel->setStyleSheet(QString(R"(
        QLabel {
            color: %1;
            font-size: 12px;
            margin: 10px 20px;
            background-color: transparent;
        }
    )").arg(textColor.name()));

    QPushButton *okBtn = new QPushButton(tr("确认"), &dialog);
    okBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #f5f5f5;
            color: black;
            border: 1px solid #cccccc;
            padding: 3px 20px;
            margin: 5px;
            border-radius: 3px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
        }
    )");

    QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);
    dialogLayout->setContentsMargins(10, 10, 10, 10);
    dialogLayout->setSpacing(15);
    dialogLayout->addWidget(contentLabel);
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addStretch();
    dialogLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    return dialog.exec();
}
