#include "moenotes_chart_parser.h"
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 65536)
        return 0;
    moenotes_parse_options_t o;
    moenotes_default_parse_options(&o);
    o.slide_combo_unit = 8;
    o.add_flick_hidden = 1;
    o.mirror = size ? data[0] & 1 : 0;
    moenotes_score_t *s = NULL;
    if (moenotes_score_parse(data, size, &o, NULL, &s, NULL, 0) == MOENOTES_OK) {
        for (size_t i = 0; i < moenotes_score_note_count(s); i++) {
            moenotes_note_view_t n;
            moenotes_score_note_at(s, i, &n);
            moenotes_position_t p;
            moenotes_score_note_position_at_tick(s, n.tick, &p);
            for (size_t j = 0; j < moenotes_score_note_line_count(s, n.id); j++) {
                int32_t line_id;
                moenotes_score_note_line_at(s, n.id, j, &line_id);
            }
        }
        for (size_t i = 0; i < moenotes_score_line_count(s); i++) {
            moenotes_line_view_t line;
            moenotes_score_line_at(s, i, &line);
            for (size_t j = 0; j < moenotes_score_line_member_count(s, line.id); j++) {
                moenotes_note_view_t n;
                moenotes_score_line_member_at(s, line.id, j, &n);
                moenotes_line_sample_t sample;
                moenotes_score_sample_line(s, line.id, n.tick, &sample);
                moenotes_score_sample_judgement_line(s, line.id, n.tick, &sample);
            }
        }
        moenotes_score_free(s);
    }
    return 0;
}
