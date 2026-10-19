#pragma once
#include <QList>
#include <QMetaType>
#include <QString>
namespace EchoMusic
{
/// Account-local automatic presentation cache; empty URL is a confirmed empty cover.
struct StoredPlaylistCover
{
    quint64 listId = 0, revision = 0;
    QString url;
    bool operator==(const StoredPlaylistCover &) const = default;
};
using StoredPlaylistCovers = QList<StoredPlaylistCover>;
} // namespace EchoMusic
Q_DECLARE_METATYPE(EchoMusic::StoredPlaylistCover)
Q_DECLARE_METATYPE(EchoMusic::StoredPlaylistCovers)
