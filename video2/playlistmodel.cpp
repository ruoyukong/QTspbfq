#include "playlistmodel.h"

PlaylistModel::PlaylistModel(QObject *parent) : QAbstractTableModel(parent) {}

int PlaylistModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_mediaList.size();
}

int PlaylistModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : 2;
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_mediaList.size()) return QVariant();
    const MediaInfo &info = m_mediaList[index.row()];
    if (role == Qt::DisplayRole) {
        return index.column() == 0 ? info.name : info.duration;
    }
    return QVariant();
}

QVariant PlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        return section == 0 ? tr("标题") : tr("长度");
    }
    return QVariant();
}

void PlaylistModel::addMedia(const MediaInfo &info) {
    beginInsertRows(QModelIndex(), m_mediaList.size(), m_mediaList.size());
    m_mediaList.append(info);
    endInsertRows();
}

void PlaylistModel::clear() {
    beginResetModel();
    m_mediaList.clear();
    endResetModel();
}

MediaInfo PlaylistModel::mediaAt(int row) const {
    return m_mediaList.value(row);
}

int PlaylistModel::mediaCount() const {
    return m_mediaList.size();
}
