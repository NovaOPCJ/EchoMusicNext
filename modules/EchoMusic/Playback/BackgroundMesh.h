#pragma once
#include <QQuickItem>
#include <QPointer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace EchoMusic
{
// Immutable Hermite geometry, sampled by a native scene-graph material. Source
// textures belong to QML's small preprocessing graph, never to this item.
class BackgroundMesh : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickItem *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(qreal phase READ phase WRITE setPhase NOTIFY phaseChanged)
    Q_PROPERTY(quint32 seed READ seed WRITE setSeed NOTIFY seedChanged)
    Q_PROPERTY(QVariantList controlPoints READ controlPoints NOTIFY seedChanged)
public:
    explicit BackgroundMesh(QQuickItem *parent = nullptr);
    QQuickItem *source() const
    {
        return m_source;
    }
    void setSource(QQuickItem *source);
    qreal phase() const
    {
        return m_phase;
    }
    void setPhase(qreal phase);
    quint32 seed() const
    {
        return m_seed;
    }
    void setSeed(quint32 seed);
    QVariantList controlPoints() const;
signals:
    void sourceChanged();
    void phaseChanged();
    void seedChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &, const QRectF &) override;

private:
    QPointer<QQuickItem> m_source;
    qreal m_phase = 0;
    quint32 m_seed = 1;
};
} // namespace EchoMusic
