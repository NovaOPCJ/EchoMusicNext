#include "EchoSelectModel.h"
#include <QFontMetricsF>
#include <QJSValue>
#include <cmath>
namespace EchoMusic
{
EchoSelectModel::EchoSelectModel(QObject *parent) : QAbstractListModel(parent) {}
int EchoSelectModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}
QHash<int, QByteArray> EchoSelectModel::roleNames() const
{
    return {{Qt::UserRole, "label"},
            {Qt::UserRole + 1, "optionValue"},
            {Qt::UserRole + 2, "optionDisabled"},
            {Qt::UserRole + 3, "sourceIndex"}};
}
QVariant EchoSelectModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count())
        return {};
    const auto &e = m_entries[m_visible[index.row()]];
    switch (role) {
    case Qt::UserRole:
        return e.label;
    case Qt::UserRole + 1:
        return e.value;
    case Qt::UserRole + 2:
        return e.disabled;
    case Qt::UserRole + 3:
        return e.sourceIndex;
    default:
        return {};
    }
}
void EchoSelectModel::setSource(const QVariant &value)
{
    for (const auto &connection : m_connections)
        disconnect(connection);
    m_connections.clear();
    m_source = value.metaType() == QMetaType::fromType<QJSValue>()
                   ? value.value<QJSValue>().toVariant()
                   : value;
    m_model = qobject_cast<QAbstractItemModel *>(m_source.value<QObject *>());
    if (m_model) {
        const auto update = [this] { rebuild(); };
        m_connections << connect(m_model, &QAbstractItemModel::modelReset, this, update)
                      << connect(m_model, &QAbstractItemModel::rowsInserted, this, update)
                      << connect(m_model, &QAbstractItemModel::rowsRemoved, this, update)
                      << connect(m_model, &QAbstractItemModel::rowsMoved, this, update)
                      << connect(m_model, &QAbstractItemModel::dataChanged, this, update)
                      << connect(m_model, &QAbstractItemModel::layoutChanged, this, update)
                      << connect(m_model, &QObject::destroyed, this, [this] {
                             m_model = nullptr;
                             m_source = {};
                             rebuild();
                         });
    }
    rebuild();
    emit sourceChanged();
}
void EchoSelectModel::setTextRole(const QString &v)
{
    if (m_textRole != v) {
        m_textRole = v;
        rebuild();
        emit sourceChanged();
    }
}
void EchoSelectModel::setValueRole(const QString &v)
{
    if (m_valueRole != v) {
        m_valueRole = v;
        rebuild();
        emit sourceChanged();
    }
}
void EchoSelectModel::setAvailable(const QVariantList &v)
{
    if (m_available != v) {
        m_available = v;
        rebuild();
        emit sourceChanged();
    }
}
void EchoSelectModel::setQuery(const QString &v)
{
    if (m_query != v) {
        m_query = v;
        filter();
        emit queryChanged();
    }
}
void EchoSelectModel::setFont(const QFont &v)
{
    if (m_font != v) {
        m_font = v;
        rebuild();
    }
}
void EchoSelectModel::rebuild()
{
    QList<Entry> entries;
    qreal labelWidth = 0;
    const QFontMetricsF metrics(m_font);
    const auto append = [&](QString label, QVariant value, bool disabled, int index) {
        labelWidth = qMax(labelWidth, metrics.horizontalAdvance(label));
        entries.append({label, value,
                        disabled || (!m_available.isEmpty() &&
                                     (index >= m_available.size() || !m_available[index].toBool())),
                        index});
    };
    if (m_model) {
        const auto roles = m_model->roleNames();
        const auto role = [&](const QString &name, int fallback) {
            return name.isEmpty() ? fallback : roles.key(name.toUtf8(), fallback);
        };
        const int labelRole =
            role(m_textRole, roles.size() == 1 ? roles.begin().key() : Qt::DisplayRole);
        const int valueRole = role(m_valueRole, labelRole), disabledRole = role("disabled", -1);
        for (int i = 0; i < m_model->rowCount(); ++i) {
            const auto idx = m_model->index(i, 0);
            append(m_model->data(idx, labelRole).toString(), m_model->data(idx, valueRole),
                   disabledRole >= 0 && m_model->data(idx, disabledRole).toBool(), i);
        }
    } else {
        const auto list = m_source.toList();
        for (int i = 0; i < list.size(); ++i) {
            auto v = list[i];
            if (v.metaType() == QMetaType::fromType<QJSValue>())
                v = v.value<QJSValue>().toVariant();
            const auto object = v.toMap();
            if (!object.isEmpty())
                append(object.value(m_textRole.isEmpty() ? "label" : m_textRole).toString(),
                       object.value(m_valueRole.isEmpty() ? "value" : m_valueRole),
                       object.value("disabled").toBool(), i);
            else
                append(v.toString(), v, false, i);
        }
    }
    // Publish atomically: aboutToReset observers may still inspect old rows.
    beginResetModel();
    m_entries = std::move(entries);
    m_labelWidth = labelWidth;
    m_visible.clear();
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].label.contains(m_query, Qt::CaseInsensitive))
            m_visible.append(i);
    endResetModel();
    emit changed();
}
void EchoSelectModel::filter()
{
    beginResetModel();
    m_visible.clear();
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].label.contains(m_query, Qt::CaseInsensitive))
            m_visible.append(i);
    endResetModel();
    emit changed();
}
QVariantMap EchoSelectModel::map(const Entry &e) const
{
    return {{"label", e.label},
            {"value", e.value},
            {"disabled", e.disabled},
            {"sourceIndex", e.sourceIndex}};
}
QVariantMap EchoSelectModel::option(int i) const
{
    return i >= 0 && i < count() ? map(m_entries[m_visible[i]]) : QVariantMap{};
}
QVariantMap EchoSelectModel::optionAtSource(int i) const
{
    return i >= 0 && i < m_entries.size() ? map(m_entries[i]) : QVariantMap{};
}
int EchoSelectModel::filteredIndex(int sourceIndex) const
{
    return m_visible.indexOf(sourceIndex);
}
QVariantMap EchoSelectModel::optionForValue(const QVariant &value) const
{
    const auto numeric = [](const QVariant &v) {
        const auto t = v.metaType().id();
        return t == QMetaType::Int || t == QMetaType::UInt || t == QMetaType::LongLong ||
               t == QMetaType::ULongLong || t == QMetaType::Double;
    };
    for (const auto &e : m_entries)
        if ((e.value.metaType() == value.metaType() && e.value == value) ||
            (numeric(e.value) && numeric(value) && e.value.toDouble() == value.toDouble()))
            return map(e);
    return {};
}
int EchoSelectModel::nextEnabled(int current, int direction) const
{
    if (direction != 1 && direction != -1)
        return current;
    for (int i = current + direction; i >= 0 && i < count(); i += direction)
        if (!m_entries[m_visible[i]].disabled)
            return i;
    return current;
}
} // namespace EchoMusic
