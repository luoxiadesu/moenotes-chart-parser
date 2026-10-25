/* Packed online snapshots list already-expanded native notes. They stay opt-in:
 * default options reject the container, and `packed_native_notes` reads the
 * node fields without deriving Combos or hidden notes. */
#include "moenotes_chart_parser.h"
#include <assert.h>
#include <string.h>

static moenotes_result_t parse(const char *json, moenotes_score_t **out) {
    moenotes_parse_options_t o;
    moenotes_default_parse_options(&o);
    o.slide_combo_unit = 8;
    char error[128];
    return moenotes_score_parse(json, strlen(json), &o, NULL, out, error, sizeof(error));
}

int main(void) {
    /* Packed snapshots are opt-in: the default options keep rejecting them. */
    const char *packed = "{\"format\":\"nnnotes.live-score/1\",\"judgementNoteCount\":2,"
                         "\"notes\":[{\"t\":0,\"pos\":2,\"size\":6,\"op\":1,\"bar\":0,"
                         "\"rhythm\":0,\"rhythmicUnit\":1920,\"timeMs\":0},"
                         "{\"t\":480,\"pos\":8,\"size\":6,\"op\":40,\"bar\":0,\"rhythm\":480,"
                         "\"rhythmicUnit\":1920,\"timeMs\":500,\"direction\":\"Left\"}]}";
    moenotes_score_t *s = NULL;
    assert(parse("{\"format\":\"nnnotes.live-score/1\",\"judgementNoteCount\":965,"
                 "\"notes\":[{\"op\":1,\"src\":[1]}]}",
                 &s) == MOENOTES_ERR_SCHEMA);
    assert(!s);
    moenotes_parse_options_t o;
    moenotes_default_parse_options(&o);
    char error[128];
    assert(moenotes_score_parse(packed, strlen(packed), &o, NULL, &s, error, sizeof(error)) ==
           MOENOTES_ERR_SCHEMA);
    assert(!s);
    o.packed_native_notes = 1;
    o.slide_combo_unit = 8;
    assert(moenotes_score_parse(packed, strlen(packed), &o, NULL, &s, error, sizeof(error)) ==
           MOENOTES_OK);
    assert(moenotes_score_note_count(s) == 2);
    assert(moenotes_score_full_combo_count(s, 1, 0) == 2);
    assert((moenotes_score_warnings(s) & MOENOTES_WARNING_PACKED_NATIVE) != 0);
    moenotes_note_view_t v;
    assert(moenotes_score_note_at(s, 1, &v) == MOENOTES_OK);
    assert(v.operate_type == MOENOTES_OP_FLICK && v.tick == 480 && v.position.time_ms == 500 &&
           v.direction == MOENOTES_DIRECTION_LEFT);
    moenotes_score_free(s);
    s = NULL;
    /* A source chart with the same notes still parses and counts judgements. */
    assert(parse("{\"events\":{\"bpm\":[{\"t\":0,\"bpm\":120}]},"
                 "\"notes\":[{\"t\":0,\"pos\":0,\"size\":6}]}",
                 &s) == MOENOTES_OK);
    assert(moenotes_score_full_combo_count(s, 1, 0) == 1);
    moenotes_score_free(s);
    s = NULL;
    /* Root events survive so annotations (Fever here) are not lost. */
    const char *with_events = "{\"format\":\"nnnotes.live-score/1\",\"events\":{"
                              "\"fever\":[[0,960]]},\"notes\":[{\"t\":0,\"pos\":2,"
                              "\"size\":6,\"op\":1,\"bar\":0,\"rhythm\":0,"
                              "\"rhythmicUnit\":1920,\"timeMs\":0}]}";
    assert(moenotes_score_parse(with_events, strlen(with_events), &o, NULL, &s, error,
                                sizeof(error)) == MOENOTES_OK);
    assert(moenotes_score_note_count(s) == 1 && moenotes_score_event_count(s) == 1);
    moenotes_score_free(s);
    return 0;
}
