#include "LyricMaterial.h"
#include <QSGMaterialShader>
#include <cstring>
namespace EchoMusic
{
namespace
{
class MaskShader final : public QSGMaterialShader
{
public:
    MaskShader()
    {
        setShaderFileName(
            VertexStage, QStringLiteral(":/qt/qml/EchoMusic/Playback/shaders/lyric-mask.vert.qsb"));
        setShaderFileName(
            FragmentStage,
            QStringLiteral(":/qt/qml/EchoMusic/Playback/shaders/lyric-mask.frag.qsb"));
    }
    bool updateUniformData(RenderState &state, QSGMaterial *material, QSGMaterial *) override
    {
        auto *m = static_cast<LyricMaterial *>(material);
        auto *data = state.uniformData();
        const auto matrix = state.combinedMatrix();
        const float opacity = state.opacity();
        memcpy(data->data(), matrix.constData(), 64);
        memcpy(data->data() + 64, &opacity, 4);
        memcpy(data->data() + 80, &m->bright, 16);
        memcpy(data->data() + 96, &m->dim, 16);
        return true;
    }
    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                            QSGMaterial *material, QSGMaterial *) override
    {
        if (binding == 1) {
            auto *mask = static_cast<LyricMaterial *>(material)->texture;
            mask->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
            *texture = mask;
        }
    }
};
} // namespace
LyricMaterial::LyricMaterial()
{
    setFlag(Blending);
}
LyricMaterial::~LyricMaterial()
{
    delete texture;
}
QSGMaterialType *LyricMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}
QSGMaterialShader *LyricMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new MaskShader;
}
int LyricMaterial::compare(const QSGMaterial *other) const
{
    // A page shares one atlas/color block; unlike pages keep distinct textures.
    return this == other ? 0 : std::less<const QSGMaterial *>()(this, other) ? -1 : 1;
}
} // namespace EchoMusic
