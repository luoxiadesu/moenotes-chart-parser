#include <moenotes_chart_parser.h>
#include <cstring>

int main() {
    const char json[] = "{\"events\":{},\"notes\":[{\"type\":\"long\","
                        "\"node\":[{\"t\":0},{\"t\":960}]}]}";
    moenotes_score_t *score = nullptr;
    const auto result = moenotes_score_parse(json, sizeof(json) - 1, nullptr, nullptr,
                                             &score, nullptr, 0);
    moenotes_position_t position{};
    moenotes_line_view_t line{};
    moenotes_line_sample_t sample{};
    const bool ok = result == MOENOTES_OK && moenotes_score_note_count(score) == 2 &&
        std::strcmp(moenotes_version_string(), MOENOTES_VERSION_STRING) == 0 &&
        moenotes_score_note_position_at_tick(score, 480, &position) == MOENOTES_OK &&
        position.time_ms == 500 &&
        moenotes_score_line_at(score, 0, &line) == MOENOTES_OK && line.begin_note_id != line.end_note_id &&
        moenotes_score_sample_judgement_line(score, 0, 480, &sample) == MOENOTES_OK && sample.width == 6;
    moenotes_score_free(score);
    return ok ? 0 : 1;
}
