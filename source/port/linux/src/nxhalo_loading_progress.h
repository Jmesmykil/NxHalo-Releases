#ifndef NXHALO_LOADING_PROGRESS_H
#define NXHALO_LOADING_PROGRESS_H
static inline float nxhalo_loading_progress(float progress,float initial)
{
    float normalized;
    if (!(progress >= 0.0f)) return 0.0f;
    if (progress >= 1.0f) return 1.0f;
    if (!(initial >= 0.0f) || initial >= 1.0f) return progress;
    normalized=(progress-initial)/(1.0f-initial);
    if (!(normalized >= 0.0f)) return 0.0f;
    return normalized > 1.0f ? 1.0f : normalized;
}
#endif
