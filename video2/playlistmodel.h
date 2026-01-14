#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractTableModel>
#include <QUrl>
#include <QString>

struct MediaInfo {
    QUrl url;
    QString name;
    QString duration;
};

class PlaylistModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit PlaylistModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void addMedia(const MediaInfo &info);
    void clear();
    MediaInfo mediaAt(int row) const;
    int mediaCount() const;

private:
    QList<MediaInfo> m_mediaList;
};

#endif // PLAYLISTMODEL_H
