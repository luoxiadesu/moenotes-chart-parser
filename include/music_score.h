#ifndef MUSIC_SCORE_H
#define MUSIC_SCORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ms_score ms_score_t;

typedef enum ms_result {
    MS_OK = 0,
    MS_ERR_INVALID_ARGUMENT,
    MS_ERR_OUT_OF_MEMORY,
    MS_ERR_GZIP,
    MS_ERR_JSON,
    MS_ERR_SCHEMA,
    MS_ERR_RANGE,
    MS_ERR_INTERNAL
} ms_result_t;

typedef void *(*ms_malloc_fn)(void *ctx, size_t size);
typedef void *(*ms_realloc_fn)(void *ctx, void *ptr, size_t size);
typedef void (*ms_free_fn)(void *ctx, void *ptr);

typedef struct ms_allocator {
    void *ctx;
    ms_malloc_fn malloc_fn;
    ms_realloc_fn realloc_fn;
    ms_free_fn free_fn;
} ms_allocator_t;

typedef struct ms_parse_options {
    int32_t start_note_id;
    uint32_t slide_combo_unit; /* 0 disables generated slide nodes. */
    uint8_t mirror;
    uint8_t add_flick_hidden;
} ms_parse_options_t;

typedef struct ms_position {
    int32_t bar;
    int32_t rhythm;
    int32_t rhythmic_unit;
    double bar_progress;
    int32_t time_ms;
} ms_position_t;

typedef enum ms_operate_type {
    MS_OP_NONE = 0,
    MS_OP_NORMAL = 1,
    MS_OP_SLIDE_BEGIN = 20,
    MS_OP_SLIDE_CONNECTION = 21,
    MS_OP_SLIDE_END = 22,
    MS_OP_FLICK = 40,
    MS_OP_SLIDE_BEGIN_FLICK = 41,
    MS_OP_SLIDE_END_FLICK = 42,
    MS_OP_TRACE = 60,
    MS_OP_SLIDE_BEGIN_TRACE = 61,
    MS_OP_SLIDE_END_TRACE = 62,
    MS_OP_SLIDE_CONNECTION_TRACE = 63,
    MS_OP_HIDDEN_SLIDE_BEGIN = 80,
    MS_OP_HIDDEN_SLIDE_END = 82,
    MS_OP_GUIDE_BEGIN = 100,
    MS_OP_GUIDE_BEGIN_NORMAL = 101,
    MS_OP_GUIDE_BEGIN_FLICK = 102,
    MS_OP_GUIDE_END = 103,
    MS_OP_GUIDE_BEGIN_TRACE = 104,
    MS_OP_GUIDE_END_TRACE = 105,
    MS_OP_COMBO = 120,
    MS_OP_COMBO_SKIP = 121,
    MS_OP_HIDDEN = 122,
    MS_OP_INVALID_HIDDEN = 123
} ms_operate_type_t;

typedef struct ms_note_view {
    int32_t id;
    ms_operate_type_t operate_type;
    int32_t tick;
    ms_position_t position;
    int32_t lane_count;
    int32_t lane_start;
    int32_t lane_end;
    double lane_start_float;
    double lane_end_float;
    double width;
    uint8_t critical;
    uint8_t visible;
    uint8_t slide_along;
    uint8_t pos_auto;
    int32_t pair_note_id;
    int32_t parent_note_id;
    int32_t hidden_for_note_id;
    int32_t line_id;
    int32_t source_index;
} ms_note_view_t;

typedef struct ms_bpm_event {
    int32_t tick;
    double bpm;
    ms_position_t position;
} ms_bpm_event_t;

typedef struct ms_signature_event {
    int32_t tick;
    int32_t numerator;
    int32_t denominator;
    ms_position_t position;
} ms_signature_event_t;

typedef struct ms_command {
    int32_t time_ms;
    int32_t current_combo;
    int32_t current_life;
    int32_t note_id;
    ms_operate_type_t operate_type;
    int32_t score_type;
} ms_command_t;

void ms_default_parse_options(ms_parse_options_t *options);
const char *ms_result_string(ms_result_t result);
const char *ms_operate_type_name(ms_operate_type_t type);
uint8_t ms_operate_type_is_judgement(ms_operate_type_t type);

ms_result_t ms_score_parse(const void *data, size_t size,
                           const ms_parse_options_t *options,
                           const ms_allocator_t *allocator,
                           ms_score_t **out_score,
                           char *error_message, size_t error_message_size);
void ms_score_free(ms_score_t *score);

size_t ms_score_note_count(const ms_score_t *score);
ms_result_t ms_score_note_at(const ms_score_t *score, size_t index,
                             ms_note_view_t *out_note);
int32_t ms_score_lane_count(const ms_score_t *score);
size_t ms_score_bpm_count(const ms_score_t *score);
ms_result_t ms_score_bpm_at(const ms_score_t *score, size_t index,
                            ms_bpm_event_t *out_event);
size_t ms_score_signature_count(const ms_score_t *score);
ms_result_t ms_score_signature_at(const ms_score_t *score, size_t index,
                                  ms_signature_event_t *out_event);
ms_result_t ms_score_position_at_tick(const ms_score_t *score, int32_t tick,
                                      ms_position_t *out_position);
size_t ms_score_line_member_count(const ms_score_t *score, int32_t line_id);
ms_result_t ms_score_line_member_at(const ms_score_t *score, int32_t line_id,
                                    size_t index, ms_note_view_t *out_note);
uint32_t ms_score_full_combo_count(const ms_score_t *score,
                                   uint8_t include_slide_combos,
                                   uint8_t include_hidden_flick_nodes);
size_t ms_score_command_count(const ms_score_t *score);
ms_result_t ms_score_command_at(const ms_score_t *score, size_t index,
                                int32_t score_type, int32_t current_life,
                                int32_t current_combo, ms_command_t *out_command);

#ifdef __cplusplus
}
#endif
#endif
