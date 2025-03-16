#ifndef PUNKT_H
#define PUNKT_H

#ifdef __cplusplus
#define EXPORT extern "C"
#else
#define EXPORT
#endif

EXPORT void punktRun(const char *graph_source_raw, const char *font_path_raw);

#undef EXPORT

#endif
