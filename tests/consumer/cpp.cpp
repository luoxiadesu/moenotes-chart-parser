#include <moenotes_chart_parser.h>
#include <cstring>

int main() {
    const char json[] = "{\"events\":{},\"notes\":[]}";
    moenotes_score_t *score = nullptr;
    const auto result = moenotes_score_parse(json, sizeof(json) - 1, nullptr, nullptr,
                                             &score, nullptr, 0);
    const bool ok = result == MOENOTES_OK && moenotes_score_note_count(score) == 0 &&
                    std::strcmp(moenotes_version_string(), MOENOTES_VERSION_STRING) == 0;
    moenotes_score_free(score);
    return ok ? 0 : 1;
}
