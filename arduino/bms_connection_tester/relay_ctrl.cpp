#include "relay_ctrl.h"
#include "bms_protocol.h"

// ---- RelaySequencer ----

void RelaySequencer::begin(const Bms2Config *cfg) {
  cfg_ = cfg;
  phase_ = PH_IDLE;
  step_ = 0;
  step_at_ = 0;
  hold_until_ = 0;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    seq_[i] = false;
    forced_[i] = false;
    forced_state_[i] = false;
  }
}

void RelaySequencer::start(unsigned long now) {
  if (!cfg_) return;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) seq_[i] = false;
  step_ = 0;
  if (cfg_->relay_mode == RELAY_ALL_ON) {
    for (uint8_t i = 0; i < RELAY_COUNT; i++) seq_[i] = true;
    enterHold(now);
    return;
  }
  // Sequential: first relay immediately, rest on the step grid.
  seq_[0] = true;
  step_ = 1;
  step_at_ = now + cfg_->step_delay_ms;
  if (step_ >= RELAY_COUNT) enterHold(now);
  else phase_ = PH_RUNNING;
}

void RelaySequencer::enterHold(unsigned long now) {
  phase_ = PH_HOLD;
  hold_until_ = (cfg_ && cfg_->hold_seconds > 0)
                    ? now + (unsigned long)cfg_->hold_seconds * 1000UL
                    : 0;
}

void RelaySequencer::stopAll() {
  phase_ = PH_IDLE;
  step_ = 0;
  hold_until_ = 0;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    seq_[i] = false;
    forced_[i] = false;
    forced_state_[i] = false;
  }
}

void RelaySequencer::setForced(uint8_t i, bool on) {
  if (i >= RELAY_COUNT) return;
  forced_[i] = true;
  forced_state_[i] = on;
}

void RelaySequencer::clearForced() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    forced_[i] = false;
    forced_state_[i] = false;
  }
}

void RelaySequencer::tick(unsigned long now) {
  if (!cfg_ || phase_ == PH_IDLE) return;
  if (phase_ == PH_RUNNING) {
    // Catch up missed steps (unsigned math is rollover-safe).
    while (step_ < RELAY_COUNT &&
         (long)(now - step_at_) >= 0) {
      seq_[step_] = true;
      step_++;
      step_at_ += cfg_->step_delay_ms;
    }
    if (step_ >= RELAY_COUNT) enterHold(now);
  } else if (phase_ == PH_HOLD) {
    if (hold_until_ != 0 && (long)(now - hold_until_) >= 0) stopAll();
  }
}

bool RelaySequencer::relayOn(uint8_t i) const {
  if (i >= RELAY_COUNT) return false;
  if (forced_[i]) return forced_state_[i];
  return seq_[i];
}

uint8_t RelaySequencer::onCount() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < RELAY_COUNT; i++)
    if (relayOn(i)) n++;
  return n;
}

// ---- DebouncedInput ----

void DebouncedInput::begin(bool initial_raw) {
  stable_ = initial_raw;
  last_raw_ = initial_raw;
  change_at_ = 0;
  fell_ = rose_ = false;
}

bool DebouncedInput::update(bool raw, unsigned long now) {
  fell_ = rose_ = false;
  if (raw != last_raw_) {
    last_raw_ = raw;
    change_at_ = now;
  } else if (raw != stable_ && (now - change_at_) >= debounce_ms_) {
    stable_ = raw;
    if (!stable_) fell_ = true;
    else rose_ = true;
  }
  return stable_;
}

// ---- SpoofWindow ----

void SpoofWindow::trigger(unsigned long now, unsigned long duration_ms) {
  if (duration_ms == 0) duration_ms = 1000;
  until_ = now + duration_ms;
  armed_ = true;
}

bool SpoofWindow::active(unsigned long now) const {
  if (!armed_) return false;
  return (long)(until_ - now) > 0;
}

// ---- spoof frame builder ----
// Golden 0x03 layout (see bms_protocol.cpp): [4:5] voltage (0.01 V),
// [6:7] current (signed, 0.01 A), [23] RSOC %, [27:30] two temps
// (raw = 2731 + 0.1C). Checksum covers [2..30], stored [31:32].
void build_spoof_frame(const Bms2Config &cfg, uint8_t out[34]) {
  for (size_t i = 0; i < BMS_RESPONSE_LEN && i < 34; i++)
    out[i] = BMS_RESPONSE[i];
  uint16_t v = (uint16_t)(cfg.spoof_v_tenth * 10u);
  uint16_t a = (uint16_t)(cfg.spoof_a_tenth * 10u);
  uint16_t t = (uint16_t)(2731u + cfg.spoof_c_tenth);
  out[4] = (uint8_t)(v >> 8);
  out[5] = (uint8_t)(v & 0xFF);
  out[6] = (uint8_t)(a >> 8);
  out[7] = (uint8_t)(a & 0xFF);
  out[23] = cfg.spoof_soc;
  out[27] = (uint8_t)(t >> 8);
  out[28] = (uint8_t)(t & 0xFF);
  out[29] = (uint8_t)(t >> 8);
  out[30] = (uint8_t)(t & 0xFF);
  uint16_t ck = jbd_checksum(&out[2], 29);  // LEN_HI+LEN_LO+27 data bytes
  out[31] = (uint8_t)(ck >> 8);
  out[32] = (uint8_t)(ck & 0xFF);
}

// ---- reply selection (mirrors main.cpp's loop exactly) ----
const uint8_t *select_reply(const JbdFrame &f, const Bms2Config &cfg,
                            bool spoof_active, const uint8_t *spoof_frame,
                            size_t &out_len) {
  size_t rl = 0;
  const uint8_t *reply = reply_for(f.reg, f.is_write, rl);
  if (!f.is_write && f.reg == 0x03 && cfg.spoof_enabled && spoof_active &&
      spoof_frame) {
    reply = spoof_frame;
    rl = SPOOF_FRAME_LEN;
  }
  out_len = rl;
  return reply;
}
