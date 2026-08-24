#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

class RotaryEncoder {
public:
    enum class LatchMode {
        FOUR3
    };

    RotaryEncoder(
        int pin1,
        int pin2,
        LatchMode mode
    )
        : pin1_(pin1),
          pin2_(pin2),
          mode_(mode) {
        lastInstance() = this;
    }

    static RotaryEncoder*& lastInstance() {
        static RotaryEncoder* instance = nullptr;
        return instance;
    }

    int getPosition() const {
        return position_;
    }

    unsigned long getMillisBetweenRotations() const {
        return millis_between_rotations_;
    }

    void tick() {
        tick_count_++;
    }

    void setPosition(int position) {
        position_ = position;
    }

    void setMillisBetweenRotations(unsigned long millis) {
        millis_between_rotations_ = millis;
    }

    int tickCount() const {
        return tick_count_;
    }

private:
    int pin1_;
    int pin2_;
    LatchMode mode_;

    int position_ = 0;
    unsigned long millis_between_rotations_ = 100;
    int tick_count_ = 0;
};

#endif