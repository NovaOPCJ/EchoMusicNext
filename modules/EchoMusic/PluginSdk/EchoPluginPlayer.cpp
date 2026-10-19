#include "EchoPluginPlayer.h"
#include "EchoPluginContext.h"
#include "PlaybackService.h"
#include <cmath>
namespace EchoMusic
{
EchoPluginReadModel::EchoPluginReadModel(QAbstractItemModel &source, QObject *parent)
    : QAbstractListModel(parent), m_source(source)
{
    connect(&source, &QAbstractItemModel::modelAboutToBeReset, this, [this] { beginResetModel(); });
    connect(&source, &QAbstractItemModel::modelReset, this, [this] { endResetModel(); });
    connect(&source, &QAbstractItemModel::rowsAboutToBeInserted, this,
            [this](const QModelIndex &, int first, int last) { beginInsertRows({}, first, last); });
    connect(&source, &QAbstractItemModel::rowsInserted, this, [this] { endInsertRows(); });
    connect(&source, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this](const QModelIndex &, int first, int last) { beginRemoveRows({}, first, last); });
    connect(&source, &QAbstractItemModel::rowsRemoved, this, [this] { endRemoveRows(); });
    connect(&source, &QAbstractItemModel::rowsAboutToBeMoved, this,
            [this](const QModelIndex &, int first, int last, const QModelIndex &, int target) {
                beginMoveRows({}, first, last, {}, target);
            });
    connect(&source, &QAbstractItemModel::rowsMoved, this, [this] { endMoveRows(); });
    connect(&source, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &first, const QModelIndex &last, const QList<int> &roles) {
                emit dataChanged(index(first.row()), index(last.row()), roles);
            });
}
int EchoPluginReadModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_source.rowCount();
}
QVariant EchoPluginReadModel::data(const QModelIndex &item, int role) const
{
    return item.isValid() ? m_source.data(m_source.index(item.row(), 0), role) : QVariant();
}
QHash<int, QByteArray> EchoPluginReadModel::roleNames() const
{
    return m_source.roleNames();
}
EchoPluginPlayer::EchoPluginPlayer(EchoPluginContext &scope, PlaybackService &host)
    : QObject(&scope), m_scope(scope), m_host(host), m_queue(*host.queue(), this),
      m_lyrics(*host.lyrics(), this)
{
    connect(&host, &PlaybackService::changed, this, &EchoPluginPlayer::changed);
    connect(&host, &PlaybackService::positionChanged, this, &EchoPluginPlayer::positionChanged);
}
QString EchoPluginPlayer::title() const
{
    return m_host.title();
}
QString EchoPluginPlayer::artist() const
{
    return m_host.artist();
}
QString EchoPluginPlayer::album() const
{
    return m_host.album();
}
QUrl EchoPluginPlayer::coverUrl() const
{
    return QUrl(m_host.cover());
}
bool EchoPluginPlayer::loading() const
{
    return m_host.loading();
}
bool EchoPluginPlayer::hasTrack() const
{
    return m_host.hasTrack();
}
QString EchoPluginPlayer::errorMessage() const
{
    return m_host.errorMessage();
}
bool EchoPluginPlayer::playing() const
{
    return m_host.playing();
}
qint64 EchoPluginPlayer::positionMs() const
{
    return m_host.positionMs();
}
qint64 EchoPluginPlayer::durationMs() const
{
    return m_host.durationMs();
}
double EchoPluginPlayer::volume() const
{
    return m_host.volume();
}
double EchoPluginPlayer::speed() const
{
    return m_host.speed();
}
EchoPluginOperation *EchoPluginPlayer::command(std::function<void()> invoke)
{
    return m_scope.execute(std::move(invoke));
}
EchoPluginOperation *EchoPluginPlayer::play()
{
    return command([this] { m_host.play(); });
}
EchoPluginOperation *EchoPluginPlayer::pause()
{
    return command([this] { m_host.pause(); });
}
EchoPluginOperation *EchoPluginPlayer::toggle()
{
    return command([this] { m_host.toggle(); });
}
EchoPluginOperation *EchoPluginPlayer::stop()
{
    return command([this] { m_host.stop(); });
}
EchoPluginOperation *EchoPluginPlayer::previous()
{
    return command([this] { m_host.previous(); });
}
EchoPluginOperation *EchoPluginPlayer::next()
{
    return command([this] { m_host.next(); });
}
EchoPluginOperation *EchoPluginPlayer::seek(qint64 position)
{
    return m_scope.execute([this, position] { m_host.seek(position); }, true);
}
EchoPluginOperation *EchoPluginPlayer::setVolume(double value)
{
    if (!std::isfinite(value) || value < 0 || value > 150)
        return m_scope.rejected("InvalidArgument", "Volume must be 0..150 percent");
    return command([this, value] { m_host.setVolume(value); });
}
EchoPluginOperation *EchoPluginPlayer::setSpeed(double value)
{
    if (!std::isfinite(value) || value < 0.1 || value > 5)
        return m_scope.rejected("InvalidArgument", "Speed must be 0.1..5");
    return command([this, value] { m_host.setSpeed(value); });
}
EchoPluginOperation *EchoPluginPlayer::playIndex(int row)
{
    return command([this, row] { m_host.playIndex(row); });
}
EchoPluginOperation *EchoPluginPlayer::remove(int row)
{
    return command([this, row] { m_host.queue()->remove(row); });
}
EchoPluginOperation *EchoPluginPlayer::move(int from, int to)
{
    return command([this, from, to] { m_host.queue()->move(from, to); });
}
EchoPluginOperation *EchoPluginPlayer::clear()
{
    return command([this] { m_host.queue()->clear(); });
}
} // namespace EchoMusic
