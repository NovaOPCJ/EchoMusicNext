#ifndef ECHOMUSIC_ECHO_DSP_PROVIDER_H
#define ECHOMUSIC_ECHO_DSP_PROVIDER_H

#include <stdint.h>

#if defined(_WIN32)
#if defined(ECHO_DSP_IMPORT_SHARED)
#define ECHO_DSP_EXPORT __declspec(dllimport)
#else
#define ECHO_DSP_EXPORT __declspec(dllexport)
#endif
#else
#define ECHO_DSP_EXPORT __attribute__((visibility("default")))
#endif

#define ECHO_DSP_ABI_VERSION 2u
#define ECHO_DSP_MODE_HEADPHONE 0u
#define ECHO_DSP_MODE_SPEAKER 1u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EchoDspConfig {
    uint32_t abi_version;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t preferred_block_frames;
    uint32_t mode;
    const char *resource_json;
    const char *preset_json;
} EchoDspConfig;

typedef struct EchoDspInfo {
    uint32_t abi_version;
    // Initial delay. Engines with configurable latency may publish the current
    // nonnegative uint32 value as top-level state JSON `latencyFrames` after
    // configure(); hosts should refresh it, retaining this value if absent.
    uint32_t latency_frames;
    uint32_t preferred_block_frames;
    uint32_t max_channels;
    const char *provider_id;
    const char *provider_version;
    const char *manifest_json;
    // Provider state may report values and ownership for its own manifest
    // controls. Host EQ bypass and final output gain are fixed host graph
    // semantics; providers must not negotiate them through a control policy.
    const char *state_json;
} EchoDspInfo;

typedef struct EchoDspApi {
    uint32_t abi_version;
    void *(*create)(const EchoDspConfig *, EchoDspInfo *);
    int (*process)(void *, float *, uint32_t frames, uint32_t channels);
    int (*drain)(void *, float *, uint32_t capacity_frames, uint32_t *written_frames);
    int (*reset)(void *);
    int (*configure)(void *, const char *preset_json);
    const char *(*get_state_json)(void *);
    void (*destroy)(void *);
} EchoDspApi;

ECHO_DSP_EXPORT const EchoDspApi *echo_dsp_get_api(void);

#ifdef __cplusplus
}
#endif

#endif
