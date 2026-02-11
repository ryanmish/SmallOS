#include "touch.h"
#include "config.h"
#include "logger.h"

// ============================================================
// Touch Implementation
// Self-calibrating capacitive touch with gesture state machine
// ============================================================

// --- Gesture state machine ---
enum TouchState {
    TOUCH_IDLE,
    TOUCH_TOUCHING
};

// --- Module state ---
static TouchState  _state          = TOUCH_IDLE;
static uint16_t    _baseline       = 0;
static uint16_t    _threshold      = 0;
static uint16_t    _lastRaw        = 0;
static bool        _paused         = false;

// Timing
static unsigned long _touchStartMs = 0;

// Event flags (cleared on read)
static bool _flagTap        = false;
static bool _flagLongPress  = false;
static bool _flagDoubleTap  = false;

// Pending tap: held between first tap and double-tap window expiry
static bool _pendingTap     = false;
static unsigned long _pendingTapMs = 0;

// Long press tracking: prevents firing twice (once while held, once on release)
static bool _longPressFired = false;

// --- Internal helpers ---

// Read the touch pin with multi-sample averaging for noise reduction.
static uint16_t readTouchAvg() {
    uint32_t sum = 0;
    for (int i = 0; i < TOUCH_SAMPLES; i++) {
        sum += touchRead(TOUCH_PIN);
    }
    return (uint16_t)(sum / TOUCH_SAMPLES);
}

// Recalculate threshold from current baseline.
static void recalcThreshold() {
    _threshold = (_baseline * TOUCH_THRESHOLD_PCT) / 100;
}

// Perform full calibration: average many samples to establish baseline.
static void calibrate() {
    uint32_t sum = 0;
    for (int i = 0; i < TOUCH_BASELINE_SAMPLES; i++) {
        sum += readTouchAvg();
        delay(5);
    }
    _baseline = (uint16_t)(sum / TOUCH_BASELINE_SAMPLES);
    recalcThreshold();
    logPrintf("Touch calibrated: baseline=%u threshold=%u (%d%%)",
              _baseline, _threshold, TOUCH_THRESHOLD_PCT);
}

// Adaptive baseline drift using exponential moving average.
// Only applied while not touching, so the baseline tracks slow
// environmental changes (temperature, humidity) without being
// pulled down by actual touches.
static void adaptBaseline(uint16_t reading) {
    // EMA with alpha = 1/16: baseline = baseline + (reading - baseline) / 16
    // Use signed math to avoid unsigned underflow when reading < baseline
    int32_t delta = (int32_t)reading - (int32_t)_baseline;
    _baseline = (uint16_t)((int32_t)_baseline + delta / 16);
    recalcThreshold();
}

// --- Public API ---

void touchInit() {
    _state = TOUCH_IDLE;
    _flagTap = false;
    _flagLongPress = false;
    _flagDoubleTap = false;
    _pendingTap = false;
    _longPressFired = false;
    _paused = false;

    calibrate();
}

void touchUpdate() {
    if (_paused) return;

    _lastRaw = readTouchAvg();
    bool isTouching = (_lastRaw < _threshold);
    unsigned long now = millis();

    // Check if pending tap's double-tap window has expired
    if (_pendingTap && !isTouching && (now - _pendingTapMs > TOUCH_DOUBLE_TAP_MS)) {
        // No second tap arrived in time, emit single tap
        _flagTap = true;
        _pendingTap = false;
        logPrintf("Touch: tap");
    }

    switch (_state) {

    case TOUCH_IDLE:
        if (isTouching) {
            _state = TOUCH_TOUCHING;
            _touchStartMs = now;
        } else {
            // Drift baseline toward current reading
            adaptBaseline(_lastRaw);
        }
        break;

    case TOUCH_TOUCHING:
        if (!isTouching) {
            // Released
            unsigned long duration = now - _touchStartMs;

            if (_longPressFired) {
                // Long press already fired while held; don't fire again on release
                _longPressFired = false;

            } else if (duration >= TOUCH_LONG_PRESS_MS) {
                // Long press (finger lifted after threshold but before held-fire)
                _flagLongPress = true;
                _pendingTap = false;
                logPrintf("Touch: long press (%lums)", duration);

            } else if (duration >= TOUCH_DEBOUNCE_MS) {
                // Valid short tap
                if (_pendingTap && (now - _pendingTapMs <= TOUCH_DOUBLE_TAP_MS)) {
                    // Second tap within window
                    _flagDoubleTap = true;
                    _pendingTap = false;
                    logPrintf("Touch: double tap");
                } else {
                    // First tap: hold as pending until double-tap window expires
                    _pendingTap = true;
                    _pendingTapMs = now;
                }
            }
            // Touches shorter than TOUCH_DEBOUNCE_MS are ignored (noise)

            _state = TOUCH_IDLE;

        } else if ((now - _touchStartMs) >= TOUCH_LONG_PRESS_MS) {
            // Still touching and long press threshold reached.
            // Fire the event immediately so the user gets feedback
            // without having to lift their finger.
            if (!_longPressFired) {
                _flagLongPress = true;
                _longPressFired = true;
                _pendingTap = false;
                logPrintf("Touch: long press (held)");
            }
        }
        break;

    default:
        _state = TOUCH_IDLE;
        break;
    }
}

bool touchWasTapped() {
    bool val = _flagTap;
    _flagTap = false;
    return val;
}

bool touchWasLongPressed() {
    bool val = _flagLongPress;
    _flagLongPress = false;
    return val;
}

bool touchWasDoubleTapped() {
    bool val = _flagDoubleTap;
    _flagDoubleTap = false;
    return val;
}

bool touchIsTouched() {
    if (_paused) return false;
    return (_lastRaw < _threshold);
}

uint16_t touchGetRaw() {
    return _lastRaw;
}

uint16_t touchGetBaseline() {
    return _baseline;
}

void touchPauseForWiFi() {
    _paused = true;
    logPrintf("Touch paused for WiFi");
}

void touchResumeAfterWiFi() {
    _paused = false;
    delay(50);  // Let ADC settle after WiFi radio activity
    calibrate();
    _state = TOUCH_IDLE;
    _flagTap = false;
    _flagLongPress = false;
    _flagDoubleTap = false;
    _pendingTap = false;
    _longPressFired = false;
    logPrintf("Touch resumed after WiFi");
}
