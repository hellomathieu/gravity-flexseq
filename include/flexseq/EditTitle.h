#ifndef FLEXSEQ_EDIT_TITLE_H
#define FLEXSEQ_EDIT_TITLE_H

#include <stdint.h>
#include <string.h>

#include <flexseq/MainScreen.h>
#include <flexseq/SequencerEngine.h>
#include <flexseq/UiController.h>

namespace flexseq {

FLEXSEQ_LABEL(LBL_EDIT_TITLE, "EDIT PATTERN A1");
FLEXSEQ_LABEL(LBL_CV_TITLE, "CV PATTERN A1");

constexpr uint8_t EDIT_TITLE_CAPACITY = 16;

static_assert(sizeof("EDIT PATTERN A1") <= EDIT_TITLE_CAPACITY,
              "the EDIT title must fit its buffer, terminator included");
static_assert(sizeof("CV PATTERN A1") <= EDIT_TITLE_CAPACITY,
              "the CV title must fit its buffer, terminator included");

inline bool editTitleFor(const UiController& ui, const SequencerEngine& engine,
                         uint8_t channel, char* out) {
    const int8_t played = engine.patternCvIndex(channel);
    if (played < 0) {
        return false;
    }
    detail::copyLabel(ui.editIsReadOnly() ? LBL_CV_TITLE : LBL_EDIT_TITLE, out);
    const uint8_t length = static_cast<uint8_t>(strlen(out));
    out[length - 2] = (played < 8) ? 'A' : 'B';
    out[length - 1] = static_cast<char>('1' + (played % 8));
    return true;
}

inline int8_t editCursorFor(const UiController& ui) {
    if (ui.isOnHeader() || ui.editIsReadOnly()) {
        return -1;
    }
    return static_cast<int8_t>(ui.stepCursor());
}

}  // namespace flexseq

#endif // FLEXSEQ_EDIT_TITLE_H
