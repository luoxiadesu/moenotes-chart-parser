#include "music_score.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f); if (n < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    unsigned char *p = malloc((size_t)n); if (!p) { fclose(f); return NULL; }
    if ((size_t)n && fread(p, 1, (size_t)n, f) != (size_t)n) { free(p); fclose(f); return NULL; }
    fclose(f); *size = (size_t)n; return p;
}

static void usage(const char *name) {
    fprintf(stderr, "usage: %s parse FILE [--mirror] [--combo-unit N]\n", name);
}

int main(int argc, char **argv) {
    if (argc < 3 || strcmp(argv[1], "parse") != 0) { usage(argv[0]); return 2; }
    ms_parse_options_t options; ms_default_parse_options(&options);
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--mirror")) options.mirror = 1;
        else if (!strcmp(argv[i], "--combo-unit") && i + 1 < argc) options.slide_combo_unit = (uint32_t)strtoul(argv[++i], NULL, 10);
        else { usage(argv[0]); return 2; }
    }
    size_t size = 0; unsigned char *data = read_file(argv[2], &size);
    if (!data) { fprintf(stderr, "cannot read %s\n", argv[2]); return 1; }
    ms_score_t *score = NULL; char error[256] = {0};
    ms_result_t result = ms_score_parse(data, size, &options, NULL, &score, error, sizeof(error));
    free(data);
    if (result != MS_OK) { fprintf(stderr, "parse failed: %s (%s)\n", ms_result_string(result), error); return 1; }
    printf("{\n  \"lane_count\": %d,\n  \"note_count\": %zu,\n  \"full_combo_count\": %u,\n  \"notes\": [\n", ms_score_lane_count(score), ms_score_note_count(score), ms_score_full_combo_count(score, 1, 0));
    for (size_t i = 0, n = ms_score_note_count(score); i < n; i++) {
        ms_note_view_t note; ms_score_note_at(score, i, &note);
        printf("    {\"id\":%d,\"type\":%d,\"type_name\":\"%s\",\"tick\":%d,\"time_ms\":%d,\"lane_start\":%d,\"lane_end\":%d,\"line_id\":%d,\"parent_note_id\":%d,\"pair_note_id\":%d,\"judgement\":%s}%s\n",
               note.id, (int)note.operate_type, ms_operate_type_name(note.operate_type), note.tick,
               note.position.time_ms, note.lane_start, note.lane_end, note.line_id,
               note.parent_note_id, note.pair_note_id,
               ms_operate_type_is_judgement(note.operate_type) ? "true" : "false",
               i + 1 == n ? "" : ",");
    }
    printf("  ]\n}\n");
    fprintf(stderr, "parsed %zu notes, full combo count %u\n", ms_score_note_count(score), ms_score_full_combo_count(score, 1, 0));
    ms_score_free(score); return 0;
}
