// Offline ABI fixture, not a ViPER implementation or a protocol sample.
#include "echo_dsp_provider.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

namespace
{
constexpr const char *manifest =
    R"({"schemaVersion":1,"displayName":"Test DSP","presets":[{"id":"gain","label":"Gain","controls":[{"id":"gain","type":"number","label":"Gain","defaultValue":2,"ownership":"provider","range":{"min":0.5,"max":2,"step":0.5}}]}]})";
struct Instance
{
    float gain = 1;
};
int configure(void *instance, const char *json)
{
    auto *p = static_cast<Instance *>(instance);
    const auto root = QJsonDocument::fromJson(json ? json : "{}").object();
    const auto id = root.value("presetId").toString();
    if (!id.isEmpty() && id != "gain")
        return -1;
    p->gain = id.isEmpty() ? 1
                           : float(root.value("controls")
                                       .toObject()
                                       .value("gain")
                                       .toObject()
                                       .value("value")
                                       .toDouble(2));
    return std::isfinite(p->gain) ? 0 : -1;
}
void *create(const EchoDspConfig *config, EchoDspInfo *info)
{
    if (!config || config->abi_version != 2 || config->channels != 2)
        return nullptr;
    auto *p = new Instance;
    if (configure(p, config->preset_json)) {
        delete p;
        return nullptr;
    }
#ifdef DSP_TEST_NO_MANIFEST
    *info = {2, 0, 512, 2, "test-dsp-bare", "1.0", nullptr, "{}"};
#else
    *info = {2, 0, 512, 2, "test-dsp", "1.0", manifest, "{}"};
#endif
    return p;
}
int process(void *instance, float *pcm, uint32_t frames, uint32_t channels)
{
    for (uint32_t i = 0; i < frames * channels; ++i)
        pcm[i] *= static_cast<Instance *>(instance)->gain;
    return 0;
}
int drain(void *, float *, uint32_t, uint32_t *written)
{
    *written = 0;
    return 0;
}
int reset(void *)
{
    return 0;
}
const char *state(void *)
{
    return "{}";
}
void destroy(void *instance)
{
    delete static_cast<Instance *>(instance);
}
const EchoDspApi api = {2, create, process, drain, reset, configure, state, destroy};
} // namespace
extern "C" ECHO_DSP_EXPORT const EchoDspApi *echo_dsp_get_api(void)
{
    return &api;
}
