#include "BackgroundMesh.h"
#include <QSGGeometryNode>
#include <QSGMaterialShader>
#include <QSGMaterial>
#include <QSGDynamicTexture>
#include <QSGTextureProvider>
#include <QVector2D>
#include <array>
#include <cmath>
#include <cstring>

namespace EchoMusic
{
namespace
{
constexpr int side = 6, divisions = 20, edge = (side - 1) * divisions + 1;
struct Point
{
    QVector2D position, u, v;
};
using Points = std::array<Point, side * side>;
Points makePoints(quint32 seed)
{
    auto random = [&] {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return float(seed & 0xffffff) / float(0xffffff) * 2 - 1;
    };
    Points points;
    constexpr float step = 2.f / (side - 1);
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
            auto &p = points[y * side + x];
            p.position = QVector2D(-1 + x * step, -1 + y * step);
            p.u = QVector2D(step, 0);
            p.v = QVector2D(0, step);
            if (x && y && x != side - 1 && y != side - 1) {
                // Sequence PRNG calls explicitly: argument evaluation order
                // differs between C++ compilers, but a cover seed must not.
                const float dx = random(), dy = random();
                p.position += QVector2D(dx, dy) * (step * .45f);
                const float a = random(), b = random();
                p.u = QVector2D(std::cos(a), std::sin(a)) * (step * (1 + random() * .2f));
                p.v = QVector2D(-std::sin(b), std::cos(b)) * (step * (1 + random() * .2f));
            }
        }
    // Smooth the generated interior without storing/copying upstream presets.
    for (int pass = 0; pass < 3; ++pass) {
        auto next = points;
        for (int y = 1; y < side - 1; ++y)
            for (int x = 1; x < side - 1; ++x) {
                QVector2D position;
                for (int j = -1; j <= 1; ++j)
                    for (int i = -1; i <= 1; ++i)
                        position += points[(y + j) * side + x + i].position *
                                    float((i == 0 ? 2 : 1) * (j == 0 ? 2 : 1));
                next[y * side + x].position =
                    points[y * side + x].position * .75f + position / 64.f;
            }
        points = next;
    }
    return points;
}
std::array<float, 4> basis(float t)
{
    const float t2 = t * t, t3 = t2 * t;
    return {2 * t3 - 3 * t2 + 1, -2 * t3 + 3 * t2, t3 - 2 * t2 + t, t3 - t2};
}
QVector2D evaluate(const Points &points, int x, int y, float s, float t)
{
    const auto a = basis(s), b = basis(t);
    QVector2D result;
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i) {
            const auto &p = points[(y + j) * side + x + i];
            result +=
                p.position * (a[i] * b[j]) + p.u * (a[i + 2] * b[j]) + p.v * (a[i] * b[j + 2]);
        }
    return result;
}
class Material;
class Shader final : public QSGMaterialShader
{
public:
    Shader()
    {
        setShaderFileName(
            VertexStage,
            QStringLiteral(":/qt/qml/EchoMusic/Playback/shaders/background-mesh.vert.qsb"));
        setShaderFileName(
            FragmentStage,
            QStringLiteral(":/qt/qml/EchoMusic/Playback/shaders/background-mesh.frag.qsb"));
    }
    bool updateUniformData(RenderState &, QSGMaterial *, QSGMaterial *) override;
    void updateSampledImage(RenderState &, int, QSGTexture **, QSGMaterial *,
                            QSGMaterial *) override;
};
class Material final : public QSGMaterial
{
public:
    Material()
    {
        setFlag(Blending);
    }
    QSGTexture *texture = nullptr;
    QVector4D pose;
    QSGMaterialType *type() const override
    {
        static QSGMaterialType type;
        return &type;
    }
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override
    {
        return new Shader;
    }
    int compare(const QSGMaterial *other) const override
    {
        return this == other ? 0 : std::less<const QSGMaterial *>()(this, other) ? -1 : 1;
    }
};
bool Shader::updateUniformData(RenderState &state, QSGMaterial *material, QSGMaterial *)
{
    auto *m = static_cast<Material *>(material);
    auto *data = state.uniformData();
    const auto matrix = state.combinedMatrix();
    const float opacity = state.opacity();
    std::memcpy(data->data(), matrix.constData(), 64);
    std::memcpy(data->data() + 64, &opacity, 4);
    std::memcpy(data->data() + 80, &m->pose, 16);
    return true;
}
void Shader::updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                                QSGMaterial *material, QSGMaterial *)
{
    if (binding != 1)
        return;
    auto *source = static_cast<Material *>(material)->texture;
    source->setFiltering(QSGTexture::Linear);
    source->setHorizontalWrapMode(QSGTexture::MirroredRepeat);
    source->setVerticalWrapMode(QSGTexture::MirroredRepeat);
    source->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
    *texture = source;
}
class MeshNode final : public QSGGeometryNode
{
public:
    QSGGeometry geometry{QSGGeometry::defaultAttributes_TexturedPoint2D(), edge * edge,
                         (edge - 1) * (edge - 1) * 6};
    Material material;
    QPointer<QSGTextureProvider> provider;
    QMetaObject::Connection changed;
    ~MeshNode() override
    {
        QObject::disconnect(changed);
    }
    QSizeF size;
    quint32 seed = 0;
    MeshNode()
    {
        setFlag(UsePreprocess);
        setGeometry(&geometry);
        setMaterial(&material);
        geometry.setDrawingMode(QSGGeometry::DrawTriangles);
        geometry.setVertexDataPattern(QSGGeometry::StaticPattern);
        geometry.setIndexDataPattern(QSGGeometry::StaticPattern);
        auto *indices = geometry.indexDataAsUShort();
        for (int y = 0; y < edge - 1; ++y)
            for (int x = 0; x < edge - 1; ++x) {
                const int i = y * edge + x;
                *indices++ = i;
                *indices++ = i + 1;
                *indices++ = i + edge;
                *indices++ = i + edge;
                *indices++ = i + 1;
                *indices++ = i + edge + 1;
            }
    }
    void preprocess() override
    {
        if (provider) {
            material.texture = provider->texture();
            if (auto *dynamic = dynamic_cast<QSGDynamicTexture *>(material.texture))
                if (dynamic->updateTexture())
                    markDirty(DirtyMaterial);
        }
    }
    void rebuild(quint32 newSeed, QSizeF newSize)
    {
        seed = newSeed;
        size = newSize;
        const auto points = makePoints(seed);
        auto *vertices = geometry.vertexDataAsTexturedPoint2D();
        const float aspect = size.width() / size.height();
        for (int y = 0; y < edge; ++y)
            for (int x = 0; x < edge; ++x) {
                const int cx = std::min(x / divisions, side - 2),
                          cy = std::min(y / divisions, side - 2);
                const auto p = evaluate(points, cx, cy, float(x - cx * divisions) / divisions,
                                        float(y - cy * divisions) / divisions);
                vertices[y * edge + x].set((p.x() / std::min(1.f, aspect) + 1) * .5f * size.width(),
                                           (1 - p.y() * std::max(1.f, aspect)) * .5f *
                                               size.height(),
                                           float(x) / (edge - 1), 1 - float(y) / (edge - 1));
            }
        geometry.markVertexDataDirty();
        markDirty(DirtyGeometry);
    }
};
} // namespace
BackgroundMesh::BackgroundMesh(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents);
}
void BackgroundMesh::setSource(QQuickItem *source)
{
    if (m_source == source)
        return;
    m_source = source;
    emit sourceChanged();
    update();
}
void BackgroundMesh::setPhase(qreal phase)
{
    if (m_phase == phase)
        return;
    m_phase = phase;
    emit phaseChanged();
    update();
}
void BackgroundMesh::setSeed(quint32 seed)
{
    seed = seed ? seed : 1;
    if (m_seed == seed)
        return;
    m_seed = seed;
    emit seedChanged();
    update();
}
QVariantList BackgroundMesh::controlPoints() const
{
    QVariantList result;
    for (const auto &p : makePoints(m_seed))
        result.append(QVariant::fromValue(
            QVariantList{p.position.x(), p.position.y(), p.u.x(), p.u.y(), p.v.x(), p.v.y()}));
    return result;
}
void BackgroundMesh::geometryChange(const QRectF &now, const QRectF &before)
{
    QQuickItem::geometryChange(now, before);
    if (now.size() != before.size())
        update();
}
QSGNode *BackgroundMesh::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    auto *provider =
        m_source && m_source->isTextureProvider() ? m_source->textureProvider() : nullptr;
    if (!provider || !provider->texture() || width() <= 0 || height() <= 0) {
        delete old;
        return nullptr;
    }
    auto *node = static_cast<MeshNode *>(old);
    if (!node)
        node = new MeshNode;
    if (node->provider != provider) {
        disconnect(node->changed);
        node->provider = provider;
        node->changed = connect(provider, &QSGTextureProvider::textureChanged, this,
                                &QQuickItem::update, Qt::QueuedConnection);
    }
    if (node->seed != m_seed || node->size != size())
        node->rebuild(m_seed, size());
    node->material.texture = provider->texture();
    const float angle = m_phase * .2;
    node->material.pose = QVector4D(std::cos(angle), std::sin(angle), 0, 0);
    node->markDirty(QSGNode::DirtyMaterial);
    return node;
}
} // namespace EchoMusic
