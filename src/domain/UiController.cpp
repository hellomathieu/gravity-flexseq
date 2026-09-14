#include <flexseq/UiController.h>

#include <flexseq/Pattern.h>
#include <flexseq/Subdiv.h>

namespace flexseq {

namespace {

uint8_t barLengthAtIndex(uint8_t index) {
    if (index == 0) return 0;
    if (index == 1) return 2;
    if (index == 2) return 3;
    if (index == 3) return 4;
    return 6;
}

uint8_t ratchetAtIndex(uint8_t index) {
    if (index == 0) return RATCHET_NONE;
    if (index == 1) return RATCHET_2;
    if (index == 2) return RATCHET_3;
    if (index == 3) return RATCHET_4;
    if (index == 4) return RATCHET_6;
    return RATCHET_TRIPLET;
}

__attribute__((noinline))
uint8_t wrapIndex(uint8_t current, int16_t delta, uint8_t count) {
    if (count == 0) {
        return 0;
    }
    const int16_t span = static_cast<int16_t>(count);
    int16_t value = static_cast<int16_t>(static_cast<int16_t>(current) + delta) % span;
    if (value < 0) {
        value = static_cast<int16_t>(value + span);
    }
    return static_cast<uint8_t>(value);
}

int16_t clampRange(int16_t value, int16_t low, int16_t high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

__attribute__((noinline))
uint8_t clampIndex(uint8_t current, int16_t delta, uint8_t count) {
    if (count == 0) {
        return 0;
    }
    const int16_t value = clampRange(
        static_cast<int16_t>(static_cast<int16_t>(current) + delta),
        0,
        static_cast<int16_t>(count - 1)
    );
    return static_cast<uint8_t>(value);
}

__attribute__((noinline))
int8_t oneStep(int8_t delta) {
    if (delta > 0) {
        return 1;
    }
    if (delta < 0) {
        return -1;
    }
    return 0;
}

int8_t indexOfChoice(uint8_t (*choiceAt)(uint8_t), uint8_t count, uint8_t value) {
    for (uint8_t index = 0; index < count; ++index) {
        if (choiceAt(index) == value) {
            return static_cast<int8_t>(index);
        }
    }
    return -1;
}

}  // namespace

UiController::UiController(SequencerEngine& engine, Transport& transport)
    : engine_(engine),
      transport_(transport),
      level_(LEVEL_TAB_BAR),
      currentTab_(TAB_FIRST_CHANNEL),
      cursor_(0),
      stepCursor_(0),
      slotCursor_(FIRST_WRITABLE_TEMPLATE),
      onHeader_(false),
      onConfigPage_(false),
      fieldOpen_(false),
      patternAction_(PATTERN_ACTION_LOAD),
      tempo_(DEFAULT_TEMPO),
      clockSource_(0),
      revision_(0) {}

bool UiController::isChannelTab() const {
    return currentTab_ >= TAB_FIRST_CHANNEL
        && currentTab_ < TAB_FIRST_CHANNEL + SequencerEngine::CHANNEL_COUNT;
}

int8_t UiController::selectedChannel() const {
    if (!isChannelTab()) {
        return -1;
    }
    return static_cast<int8_t>(currentTab_ - TAB_FIRST_CHANNEL);
}

bool UiController::isLegacyModeTab() const {
    const int8_t channel = selectedChannel();
    if (channel < 0) {
        return false;
    }
    const ChannelMode mode = engine_.getChannelMode(static_cast<uint8_t>(channel));
    return mode == MODE_CLOCK || mode == MODE_RANDOM;
}

// SAVE n apparait que si la copie du canal differe du template qu il a charge —
// PRD 12.9 point 5. Un moteur non cable n a pas de drapeau : il ne propose donc
// que LOAD, et il ne lit jamais un pointeur nul.
uint8_t UiController::patternActionCount() const {
    const ModulatedPatternState* modulated = engine_.modulatedPatterns();
    const int8_t channel = selectedChannel();
    if (modulated == nullptr || channel < 0) {
        return 1;
    }
    return modulated->isDirty(static_cast<uint8_t>(channel))
        ? PATTERN_ACTION_COUNT : 1;
}

uint8_t UiController::fieldCount() const {
    if (currentTab_ == TAB_CLOCK) {
        return CLOCK_TAB_FIELDS;
    }
    if (isChannelTab()) {
        if (onConfigPage_) {
            return CONFIG_PAGE_FIELDS;
        }
        // La grande valeur n existe qu en SEQ : les deux autres modes ne lisent
        // aucun pattern.
        return isLegacyModeTab() ? CHANNEL_TAB_FIELDS : SEQ_CHANNEL_TAB_FIELDS;
    }
    if (currentTab_ == TAB_PATTERNS) {
        return PATTERNS_TAB_FIELDS;
    }
    return 0;
}

UiController::Field UiController::fieldAt(uint8_t index) const {
    if (index >= fieldCount()) {
        return FIELD_NONE;
    }
    if (currentTab_ == TAB_CLOCK) {
        return index == 0 ? FIELD_TEMPO : FIELD_CLOCK_SOURCE;
    }
    if (currentTab_ == TAB_PATTERNS) {
        // Une seule ligne : l etat de l emplacement vit sous la grande valeur,
        // et l emplacement se change par SHIFT plus rotation depuis la barre,
        // comme le pattern d un canal.
        return FIELD_EDIT_ENTRY;
    }
    if (onConfigPage_) {
        switch (index) {
            case CONFIG_FIELD_INDEX_LENGTH: return FIELD_LENGTH;
            case CONFIG_FIELD_INDEX_SUBDIV: return FIELD_SUBDIV;
            default: return FIELD_MOD;
        }
    }
    if (isLegacyModeTab()) {
        const int8_t channel = selectedChannel();
        const ChannelMode mode = engine_.getChannelMode(static_cast<uint8_t>(channel));
        switch (index) {
            case 0: return FIELD_MODE;
            case 1: return mode == MODE_CLOCK ? FIELD_OFFSET : FIELD_SUBDIV;
            default: return FIELD_MOD;
        }
    }
    switch (index) {
        case SEQ_FIELD_INDEX_PATTERN: return FIELD_PATTERN;
        case SEQ_FIELD_INDEX_MODE: return FIELD_MODE;
        case SEQ_FIELD_INDEX_EDIT_ENTRY: return FIELD_EDIT_ENTRY;
        default: return FIELD_CONFIG;
    }
}

Pattern* UiController::currentPattern() const {
    // ADR 0013 : dans l editeur de templates, l edition tombe dans le tampon du
    // canal d audition. Hors de l editeur cet onglet n edite rien.
    if (currentTab_ == TAB_PATTERNS) {
        return level_ == LEVEL_EDIT
            ? engine_.patternForChannel(ModulatedPatternState::EDITOR_CHANNEL)
            : nullptr;
    }
    const int8_t channel = selectedChannel();
    if (channel < 0) {
        return nullptr;
    }
    return engine_.instanceForChannel(static_cast<uint8_t>(channel));
}

void UiController::handle(Event event, int8_t delta) {
    ++revision_;
    if (event == EVENT_PLAY_PRESS) {
        togglePlay();
        return;
    }
    if (event == EVENT_SHIFT_PRESS || event == EVENT_SHIFT_PLAY_PRESS) {
        return;
    }
    switch (level_) {
        case LEVEL_TAB_BAR: handleTabBar(event, delta); break;
        case LEVEL_TAB: handleTab(event, delta); break;
        case LEVEL_EDIT: handleEdit(event, delta); break;
    }
}

void UiController::handleTabBar(Event event, int8_t delta) {
    switch (event) {
        case EVENT_ROTATE:
            currentTab_ = wrapIndex(currentTab_, oneStep(delta), TAB_COUNT);
            cursor_ = 0;
            fieldOpen_ = false;
            break;
        case EVENT_SHIFT_ROTATE:
            adjustFieldValue(mainField(), delta);
            break;
        case EVENT_PRESS:
            if (fieldCount() > 0) {
                level_ = LEVEL_TAB;
                cursor_ = 0;
                fieldOpen_ = false;
            }
            break;
        default:
            break;
    }
}

void UiController::handleTab(Event event, int8_t delta) {
    switch (event) {
        case EVENT_ROTATE:
            if (fieldOpen_ && field() == FIELD_PATTERN) {
                // Le champ ouvert choisit une ACTION. Le NUMERO du template se
                // nomme par SHIFT plus rotation, et ce geste ne change pas.
                patternAction_ = clampIndex(patternAction_, oneStep(delta),
                                            patternActionCount());
            } else if (fieldOpen_) {
                adjustField(delta);
            } else {
                cursor_ = wrapIndex(cursor_, oneStep(delta), fieldCount());
            }
            break;
        case EVENT_SHIFT_ROTATE:
            adjustField(delta);
            break;
        case EVENT_PRESS:
            if (fieldOpen_) {
                fieldOpen_ = false;
            } else if (field() == FIELD_EDIT_ENTRY) {
                level_ = LEVEL_EDIT;
                stepCursor_ = 0;
                onHeader_ = false;
            } else if (field() == FIELD_CONFIG) {
                onConfigPage_ = true;
                cursor_ = CONFIG_FIELD_INDEX_LENGTH;
            } else if (field() != FIELD_NONE) {
                fieldOpen_ = true;
                patternAction_ = PATTERN_ACTION_LOAD;
            }
            break;
        case EVENT_LONG_PRESS:
            if (fieldOpen_) {
                fieldOpen_ = false;
            } else if (onConfigPage_) {
                onConfigPage_ = false;
                cursor_ = SEQ_FIELD_INDEX_CONFIG;
            } else {
                level_ = LEVEL_TAB_BAR;
            }
            break;
        default:
            break;
    }
}

void UiController::handleEdit(Event event, int8_t delta) {
    const int8_t step = oneStep(delta);
    switch (event) {
        case EVENT_ROTATE:
            if (onHeader_) {
                if (fieldOpen_) {
                    if (currentTab_ == TAB_PATTERNS) {
                        adjustTemplateLength(delta);
                    } else {
                        adjustFieldValue(FIELD_BAR_LENGTH, delta);
                    }
                } else if (step > 0) {
                    onHeader_ = false;
                    stepCursor_ = 0;
                }
            } else if (step < 0 && stepCursor_ == 0) {
                onHeader_ = true;
            } else {
                stepCursor_ = wrapIndex(stepCursor_, step, STEP_COUNT);
            }
            break;
        case EVENT_PRESS:
            if (onHeader_) {
                fieldOpen_ = !fieldOpen_;
            } else {
                toggleStep();
            }
            break;
        case EVENT_LONG_PRESS:
            if (onHeader_) {
                onHeader_ = false;
                fieldOpen_ = false;
                stepCursor_ = 0;
            } else {
                level_ = LEVEL_TAB;
                fieldOpen_ = false;
            }
            break;
        case EVENT_SHIFT_ROTATE:
            if (!onHeader_) {
                adjustRatchet(delta);
            }
            break;
        case EVENT_SHIFT_LONG_PRESS:
            clearPattern();
            break;
        default:
            break;
    }
}

bool UiController::setTempo(uint16_t bpm) {
    if (bpm < MIN_TEMPO || bpm > MAX_TEMPO) {
        return false;
    }
    tempo_ = bpm;
    ++revision_;
    return true;
}

bool UiController::setClockSource(uint8_t source) {
    if (source >= CLOCK_SOURCE_COUNT) {
        return false;
    }
    clockSource_ = source;
    ++revision_;
    return true;
}

void UiController::adjustField(int8_t delta) {
    adjustFieldValue(field(), delta);
}

UiController::Field UiController::mainField() const {
    if (currentTab_ == TAB_CLOCK) {
        return FIELD_TEMPO;
    }
    if (currentTab_ == TAB_PATTERNS) {
        return FIELD_SLOT;
    }
    const int8_t channel = selectedChannel();
    if (channel < 0) {
        return FIELD_NONE;
    }
    switch (engine_.getChannelMode(static_cast<uint8_t>(channel))) {
        case MODE_CLOCK:  return FIELD_SUBDIV;
        case MODE_RANDOM: return FIELD_SKIP_CHANCE;
        default:          return FIELD_PATTERN;
    }
}

void UiController::adjustFieldValue(Field target, int8_t raw) {
    const int8_t delta = oneStep(raw);

    if (target == FIELD_TEMPO) {
        tempo_ = static_cast<uint16_t>(clampRange(
            static_cast<int16_t>(static_cast<int16_t>(tempo_) + delta),
            static_cast<int16_t>(MIN_TEMPO),
            static_cast<int16_t>(MAX_TEMPO)
        ));
        return;
    }
    if (target == FIELD_CLOCK_SOURCE) {
        clockSource_ = clampIndex(clockSource_, delta, CLOCK_SOURCE_COUNT);
        return;
    }
    if (target == FIELD_SLOT) {
        slotCursor_ = static_cast<uint8_t>(clampRange(
            static_cast<int16_t>(static_cast<int16_t>(slotCursor_) + delta),
            static_cast<int16_t>(FIRST_WRITABLE_TEMPLATE),
            static_cast<int16_t>(SequencerEngine::PATTERN_COUNT - 1)
        ));
        return;
    }

    const int8_t selected = selectedChannel();
    if (selected < 0) {
        return;
    }
    const uint8_t ch = static_cast<uint8_t>(selected);

    switch (target) {
        case FIELD_PATTERN: {
            const int8_t current = engine_.getSelectedPattern(ch);
            if (current < 0) {
                break;
            }
            engine_.setSelectedPattern(
                ch, clampIndex(static_cast<uint8_t>(current), delta,
                               SequencerEngine::PATTERN_COUNT));
            break;
        }
        case FIELD_LENGTH:
            engine_.setBaseLength(ch, static_cast<uint8_t>(clampRange(
                static_cast<int16_t>(engine_.getBaseLength(ch) + delta),
                static_cast<int16_t>(SequencerEngine::MIN_LENGTH),
                static_cast<int16_t>(SequencerEngine::MAX_LENGTH))));
            markTemplateEdited();
            break;
        case FIELD_SUBDIV: {
            int8_t index = subdivIndexOf(engine_.getSubdiv(ch));
            if (index < 0) {
                index = static_cast<int8_t>(DEFAULT_SUBDIV_INDEX);
            }
            engine_.setSubdiv(ch, subdivAtIndex(clampIndex(
                static_cast<uint8_t>(index), delta, SUBDIV_CHOICE_COUNT)));
            break;
        }
        case FIELD_MODE: {
            const uint8_t next = clampIndex(
                static_cast<uint8_t>(engine_.getChannelMode(ch)), delta,
                CHANNEL_MODE_COUNT);
            engine_.setChannelMode(ch, static_cast<ChannelMode>(next));
            // ⚠️ La position 0 change de sens avec le mode : elle porte MODE
            // hors SEQ et la grande valeur en SEQ. Sans ce recalage le curseur
            // resterait immobile pendant que le champ sous lui changerait, et un
            // appui court ouvrirait LOAD au lieu de MODE. C est le seul endroit
            // du firmware ou un geste deplace ce que le curseur designe.
            cursor_ = isLegacyModeTab() ? 0 : SEQ_FIELD_INDEX_MODE;
            break;
        }
        case FIELD_MOD: {
            if (isLegacyModeTab()) {
                break;
            }
            int8_t index = modIndexOf(engine_.getCvDestination(ch, CV_SOURCE_1),
                                      engine_.getCvDestination(ch, CV_SOURCE_2));
            if (index < 0) {
                index = 0;
            }
            CvDestination first = CV_DEST_NONE;
            CvDestination second = CV_DEST_NONE;
            modChoiceAt(clampIndex(static_cast<uint8_t>(index), delta,
                                   MOD_CHOICE_COUNT),
                        &first, &second);
            engine_.setCvDestination(ch, CV_SOURCE_1, first);
            engine_.setCvDestination(ch, CV_SOURCE_2, second);
            break;
        }
        case FIELD_OFFSET: {
            const int16_t candidate = static_cast<int16_t>(
                static_cast<int16_t>(engine_.getOffset(ch)) + delta);
            engine_.setOffset(ch, static_cast<uint16_t>(candidate < 0 ? 0 : candidate));
            break;
        }
        case FIELD_SKIP_CHANCE:
            engine_.setSkipChance(ch, clampIndex(
                engine_.getSkipChance(ch), delta,
                static_cast<uint8_t>(MAX_SKIP_CHANCE_SETTING + 1)));
            break;
        case FIELD_BAR_LENGTH: {
            const int8_t current = engine_.getBarLength(ch);
            if (current < 0) {
                break;
            }
            int8_t index = indexOfChoice(
                barLengthAtIndex, BAR_LENGTH_CHOICE_COUNT,
                static_cast<uint8_t>(current));
            if (index < 0) {
                index = 0;
            }
            engine_.setBarLength(ch, barLengthAtIndex(clampIndex(
                static_cast<uint8_t>(index), delta, BAR_LENGTH_CHOICE_COUNT)));
            break;
        }
        default:
            break;
    }
}

void UiController::adjustRatchet(int8_t delta) {
    Pattern* pattern = currentPattern();
    if (pattern == nullptr) {
        return;
    }
    bool active = false;
    if (!pattern->readStep(stepCursor_, active) || !active) {
        return;
    }
    // Un ratchet doit tenir dans le pas : il faut une cadence, donc un canal.
    // L onglet PATTERNS n en selectionne aucun, et c est le canal d audition qui
    // la donne — le meme qui joue le template (ADR 0013).
    const int8_t channel = currentTab_ == TAB_PATTERNS
        ? static_cast<int8_t>(ModulatedPatternState::EDITOR_CHANNEL)
        : selectedChannel();
    if (channel < 0) {
        return;
    }
    const int8_t step = oneStep(delta);
    if (step == 0) {
        return;
    }
    const uint16_t ticks = engine_.getTicksPerStep(static_cast<uint8_t>(channel));

    int8_t index = indexOfChoice(
        ratchetAtIndex, RATCHET_CHOICE_COUNT, pattern->getRatchet(stepCursor_)
    );
    if (index < 0) {
        index = 0;
    }

    uint8_t cursor = static_cast<uint8_t>(index);
    for (uint8_t tried = 0; tried < RATCHET_CHOICE_COUNT; ++tried) {
        const uint8_t candidate = clampIndex(cursor, step, RATCHET_CHOICE_COUNT);
        if (candidate == cursor) {
            return;
        }
        cursor = candidate;
        if (ratchetFitsStep(ratchetAtIndex(cursor), ticks)) {
            pattern->setRatchet(stepCursor_, ratchetAtIndex(cursor));
            engine_.refreshTiming(static_cast<uint8_t>(channel));
            return;
        }
    }
}

void UiController::togglePlay() {
    if (clockSource_ != CLOCK_SOURCE_INTERNAL) {
        return;
    }
    if (engine_.isRunning()) {
        transport_.stop();
    } else {
        transport_.start();
    }
}

void UiController::toggleStep() {
    Pattern* pattern = currentPattern();
    if (pattern == nullptr) {
        return;
    }
    bool active = false;
    if (!pattern->readStep(stepCursor_, active)) {
        return;
    }
    pattern->writeStep(stepCursor_, !active);
    markTemplateEdited();
}

void UiController::clearPattern() {
    Pattern* pattern = currentPattern();
    if (pattern == nullptr) {
        return;
    }
    pattern->clear();
    engine_.refreshTiming();
    markTemplateEdited();
}

// ADR 0013 : l editeur de templates ecrit son enregistrement en sortant, et
// seulement s il a change. Le drapeau vit avec le tampon.
// Les deux drapeaux ne se confondent pas : dans l editeur de templates c est le
// TEMPLATE qui change, ailleurs c est la COPIE d un canal. Un seul point de pose
// pour les deux, parce que les quatre sites d edition passent tous par ici.
void UiController::markTemplateEdited() {
    ModulatedPatternState* modulated = engine_.modulatedPatterns();
    if (modulated == nullptr) {
        return;
    }
    if (currentTab_ == TAB_PATTERNS && level_ == LEVEL_EDIT) {
        modulated->editorDirty = 1;
        return;
    }
    const int8_t channel = selectedChannel();
    if (channel >= 0) {
        modulated->markDirty(static_cast<uint8_t>(channel));
    }
}

void UiController::adjustTemplateLength(int8_t delta) {
    ModulatedPatternState* modulated = engine_.modulatedPatterns();
    if (modulated == nullptr) {
        return;
    }
    const uint8_t ch = ModulatedPatternState::EDITOR_CHANNEL;
    const uint8_t next = static_cast<uint8_t>(clampRange(
        static_cast<int16_t>(modulated->length[ch] + oneStep(delta)),
        static_cast<int16_t>(SequencerEngine::MIN_LENGTH),
        static_cast<int16_t>(SequencerEngine::MAX_LENGTH)));
    modulated->length[ch] = next;
    (void)engine_.setBaseLength(ch, next);
    markTemplateEdited();
}

}  // namespace flexseq
