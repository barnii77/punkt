#ifndef PUNKT_H
#define PUNKT_H

#ifdef __cplusplus
#define EXPORT extern "C"
#else
#define EXPORT
#endif

EXPORT void punktRun(const char *graph_source_cstr, const char *font_path_relative_to_project_root_cstr);

#undef EXPORT

#endif
