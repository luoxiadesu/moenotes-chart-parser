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
        }
        moenotes_score_free(s);
    }
    return 0;
}
