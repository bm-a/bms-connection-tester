#include "relay_ctrl.h"
#include "bms_protocol.h"

// ---- RelaySequencer ----

void RelaySequencer::begin(const Bms2Config *cfg) {
  cfg_ = cfg;
  phase_ = PH_IDLE;
  step_ = 0;
  step_at_ = 0;
  hold_until_ = 0;
  pause_until_ = 0;
  cycles_done_ = 0;
  acts_ = 0;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    seq_[i] = false;
    forced_[i] = false;
    forced_state_[i] = false;
  }
}

uint8_t RelaySequencer::mapStep(uint8_t k) const {
  uint8_t n = effCount();
  if (k >= n) k = n - 1;
  if (cfg_ && cfg_->seq_dir) return (uint8_t)(n - 1 - k);  // reverse sweep
  return k;
}

void RelaySequencer::lightStep(uint8_t k) {
  uint8_t idx = mapStep(k);
  if (!seq_[idx]) acts_++;  // count turn-ON edges only
  seq_[idx] = true;
}

void RelaySequencer::enterPause(unsigned long now) {
  phase_ = PH_PAUSE;
  pause_until_ = now + (cfg_ ? cfg_->cycle_pause_ms : 0);
}

uint16_t RelaySequencer::stepGap() const {
  if (!cfg_) return 500;
  if (cfg_->relay_mode == RELAY_ALL_ON && cfg_->allon_stagger_ms > 0)
    return cfg_->allon_stagger_ms;
  return cfg_->step_delay_ms;
}

void RelaySequencer::start(unsigned long now) {
  if (!cfg_) return;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) seq_[i] = false;
  step_ = 0;
  uint8_t n = effCount();
  // Step gap: ALL-ON with stagger ramps like sequential (inrush limiting).
  uint16_t gap = stepGap();
  bool ramp_all = (cfg_->relay_mode == RELAY_ALL_ON &&
                   cfg_->allon_stagger_ms > 0);
  if (cfg_->relay_mode == RELAY_ALL_ON && !ramp_all) {
    for (uint8_t i = 0; i < n; i++) lightStep(i);
    enterHold(now);
    return;
  }
  if (cfg_->relay_mode == RELAY_CHASE) {
    // Chase: single lit relay sweeping across R1..R(n), wraps until hold
    // expires (or forever when hold is 0). Button modes still apply.
    for (uint8_t i = 0; i < RELAY_COUNT; i++) seq_[i] = false;
    lightStep(0);
    step_ = 1 % n;
    step_at_ = now + gap;
    phase_ = PH_CHASE;
    uint32_t h = holdForMode();
    hold_until_ = h > 0 ? now + h : 0;
    return;
  }
  // Sequential (or staggered ALL-ON): first relay immediately, rest on grid.
  lightStep(0);
  step_ = 1;
  step_at_ = now + gap;
  if (step_ >= n) enterHold(now);
  else phase_ = PH_RUNNING;
}

void RelaySequencer::enterHold(unsigned long now) {
  phase_ = PH_HOLD;
  uint32_t h = holdForMode();
  hold_until_ = (cfg_ && h > 0) ? now + h : 0;
}

uint32_t RelaySequencer::holdForMode() const {
  if (!cfg_) return 0;
  if (cfg_->relay_mode == RELAY_ALL_ON) return cfg_->hold_all_ms;
  if (cfg_->relay_mode == RELAY_CHASE) return cfg_->hold_chase_ms;
  return cfg_->hold_seq_ms;
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
  uint8_t n = effCount();
  uint16_t gap = stepGap();
  if (phase_ == PH_RUNNING) {
    // Catch up missed steps (unsigned math is rollover-safe).
    while (step_ < n && (long)(now - step_at_) >= 0) {
      lightStep(step_);
      step_++;
      step_at_ += gap;
    }
    if (step_ >= n) enterHold(now);
  } else if (phase_ == PH_CHASE) {
    // Advance the single lit relay; wrap R(n) -> R1. Hold expiry (if any)
    // stops the sweep; hold 0 = sweep forever until stopped.
    while ((long)(now - step_at_) >= 0) {
      for (uint8_t i = 0; i < RELAY_COUNT; i++) seq_[i] = false;
      lightStep(step_);
      step_++;
      if (step_ >= n) step_ = 0;
      step_at_ += gap;
    }
    if (hold_until_ != 0 && (long)(now - hold_until_) >= 0) cycleDone(now);
  } else if (phase_ == PH_HOLD) {
    if (hold_until_ != 0 && (long)(now - hold_until_) >= 0) cycleDone(now);
  } else if (phase_ == PH_PAUSE) {
    // Loop rest window: restart the cycle when the pause elapses.
    if ((long)(now - pause_until_) >= 0) start(now);
  }
}

// A cycle completed (hold expired): loop on (pause -> restart, honoring the
// limit) or stop. Single-shot behavior without loop_enabled is unchanged.
void RelaySequencer::cycleDone(unsigned long now) {
  cycles_done_++;
  if (cfg_ && cfg_->loop_enabled &&
      (cfg_->cycle_limit == 0 || cycles_done_ < cfg_->cycle_limit)) {
    for (uint8_t i = 0; i < RELAY_COUNT; i++) seq_[i] = false;
    enterPause(now);
    return;
  }
  stopAll();
}

uint8_t RelaySequencer::effCount() const {
  if (!cfg_) return RELAY_COUNT;
  if (cfg_->relay_count < 1) return 1;
  if (cfg_->relay_count > RELAY_COUNT) return RELAY_COUNT;
  return cfg_->relay_count;
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

// ---- SpoofPlan (v2.3 two-stage) ----

void SpoofPlan::trigger(unsigned long now, unsigned long stage1_ms,
                        unsigned long stage2_ms) {
  if (stage1_ms == 0) stage1_ms = 1000;
  if (stage2_ms == 0) stage2_ms = 1000;
  t0_ = now;
  s1_ = stage1_ms;
  s2_ = stage2_ms;
  armed_ = true;
}

uint8_t SpoofPlan::stage(unsigned long now) const {
  if (!armed_) return 0;
  unsigned long el = now - t0_;  // unsigned: rollover-safe
  if (el < s1_) return 1;
  if (el - s1_ < s2_) return 2;
  return 0;
}

// ---- spoof frame builder ----
// Golden 0x03 layout (see bms_protocol.cpp): [4:5] voltage (0.01 V),
// [6:7] current (signed, 0.01 A), [23] RSOC %, [27:30] two temps
// (raw = 2731 + 0.1C). Checksum covers [2..30], stored [31:32].
void build_spoof_frame(const Bms2Config &cfg, uint8_t stage, uint8_t out[34]) {
  uint16_t v10 = (stage == 2) ? cfg.s2_v_tenth : cfg.spoof_v_tenth;
  uint16_t a10 = (stage == 2) ? cfg.s2_a_tenth : cfg.spoof_a_tenth;
  uint16_t c10 = (stage == 2) ? cfg.s2_c_tenth : cfg.spoof_c_tenth;
  uint8_t soc = (stage == 2) ? cfg.s2_soc : cfg.spoof_soc;
  for (size_t i = 0; i < BMS_RESPONSE_LEN && i < 34; i++)
    out[i] = BMS_RESPONSE[i];
  uint16_t v = (uint16_t)(v10 * 10u);
  uint16_t a = (uint16_t)(a10 * 10u);
  uint16_t t = (uint16_t)(2731u + c10);
  out[4] = (uint8_t)(v >> 8);
  out[5] = (uint8_t)(v & 0xFF);
  out[6] = (uint8_t)(a >> 8);
  out[7] = (uint8_t)(a & 0xFF);
  out[23] = soc;
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
                            uint8_t spoof_stage, const uint8_t *spoof_frame,
                            size_t &out_len) {
  size_t rl = 0;
  const uint8_t *reply = reply_for(f.reg, f.is_write, rl);
  if (!f.is_write && f.reg == 0x03 && cfg.spoof_enabled && spoof_stage != 0 &&
      spoof_frame) {
    reply = spoof_frame;
    rl = SPOOF_FRAME_LEN;
  }
  out_len = rl;
  return reply;
}
