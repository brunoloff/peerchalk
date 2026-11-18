/* C interface of the PeerChalk math engine (src/math/engine). */
#ifndef PEERCHALK_MATH_H
#define PEERCHALK_MATH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Triangles sharing one colour: index_count entries of indices from first_index. */
typedef struct PeerChalkMathRun {
    uint32_t first_index;
    uint32_t index_count;
    uint32_t rgba; /* 0xRRGGBBAA */
} PeerChalkMathRun;

/* A laid-out formula. Lengths are in em; x grows right and y grows down from the top of
 * the formula, so its baseline is at y = height. When error is not null nothing else is
 * set and error holds a UTF-8 message. Everything belongs to the mesh. */
typedef struct PeerChalkMathMesh {
    const float *vertices; /* x, y pairs */
    size_t vertex_count;
    const uint32_t *indices;
    size_t index_count;
    const PeerChalkMathRun *runs;
    size_t run_count;
    double width;
    double height;
    double depth;
    const char *error;
} PeerChalkMathMesh;

/* Lays out tex (UTF-8, not NUL-terminated). display selects display style, rgba is the
 * default colour and tolerance the largest curve error in em. Never returns null; free
 * the result with peerchalk_math_free. Safe to call with hostile input. */
PeerChalkMathMesh *peerchalk_math_layout(const char *tex, size_t tex_length, int display,
                                         uint32_t rgba, float tolerance);
void peerchalk_math_free(PeerChalkMathMesh *mesh);

#ifdef __cplusplus
}
#endif

#endif
