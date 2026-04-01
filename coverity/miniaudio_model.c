/*
 * Coverity modeling file for miniaudio.
 *
 * Teaches Coverity about the acquire/release semantics of the miniaudio
 * engine and sound init/uninit functions so that resource leaks — such as
 * calling exit() or returning without ma_engine_uninit() / ma_sound_uninit()
 * — are detected during scans.
 *
 * Pass to cov-analyze with: --modeling-file coverity/miniaudio_model.c
 */

typedef int ma_result;
/* MA_SUCCESS == 0 */

void __coverity_resource_acquire__(void);
void __coverity_resource_release__(void);

typedef struct ma_engine        ma_engine;
typedef struct ma_engine_config ma_engine_config;
typedef struct ma_sound         ma_sound;

ma_result ma_engine_init(const ma_engine_config *pConfig, ma_engine *pEngine)
{
  ma_result r;
  if (r == 0)
    __coverity_resource_acquire__();
  return r;
}

void ma_engine_uninit(ma_engine *pEngine)
{
  __coverity_resource_release__();
}

ma_result ma_sound_init_from_file(ma_engine *pEngine, const char *pFilePath,
                                  unsigned int flags, void *pGroup,
                                  void *pAllocationCallbacks, ma_sound *pSound)
{
  ma_result r;
  if (r == 0)
    __coverity_resource_acquire__();
  return r;
}

void ma_sound_uninit(ma_sound *pSound)
{
  __coverity_resource_release__();
}
