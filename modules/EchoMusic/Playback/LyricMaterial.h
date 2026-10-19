#pragma once
#include <QSGMaterial>
#include <QSGTexture>
#include <QVector4D>
namespace EchoMusic
{
/// Render-thread owned material. Mask texture is immutable between relayouts;
/// playback frames change vertex poses and this shared color uniform block.
class LyricMaterial final : public QSGMaterial
{
public:
    LyricMaterial();
    ~LyricMaterial() override;
    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;
    int compare(const QSGMaterial *other) const override;
    QSGTexture *texture = nullptr;
    QVector4D bright, dim;
};
} // namespace EchoMusic
