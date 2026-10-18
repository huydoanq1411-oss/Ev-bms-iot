// Demo heuristics, not calibrated battery safety limits. Time is monotonic milliseconds.
class AnomalyDetector {
  constructor() { this.reset(); }
  reset() {
    this.samples = [];
    this.thermalCount = 0;
    this.sagUntil = 0;
    this.sag = null;
  }
  update(d, now) {
    const previous = this.samples.at(-1);
    if (!Number.isFinite(now) || !['v', 'i', 't'].every(k => Number.isFinite(d[k]))) {
      this.reset();
      return { ready: false, invalid: true, thermal: false, sag: false, rate: null };
    }
    if (previous && (now <= previous.time || now - previous.time > 5000)) this.reset();
    const prev = this.samples.at(-1);
    this.samples.push({ time: now, t: d.t, v: d.v, i: d.i });
    while (this.samples.length > 1 && this.samples[0].time < now - 10000) this.samples.shift();
    const first = this.samples[0];
    const elapsed = now - first.time;
    const ready = elapsed >= 5000;
    const rise = d.t - first.t;
    const rate = ready ? rise * 60000 / elapsed : null;
    this.thermalCount = ready && rate >= 6 && rise >= 0.5 ? this.thermalCount + 1 : 0;
    // Compare load magnitude without treating a charge/discharge reversal as a load step.
    if (prev && now - prev.time >= 500 && now - prev.time <= 3000 &&
        prev.i * d.i >= 0 && Math.abs(d.i) - Math.abs(prev.i) >= 2 && prev.v - d.v >= 0.3) {
      this.sagUntil = now + 10000;
      this.sag = { drop: prev.v - d.v, loadStep: Math.abs(d.i) - Math.abs(prev.i) };
    }
    return { ready, rate, thermal: this.thermalCount >= 3,
      sag: now < this.sagUntil, drop: this.sag?.drop, loadStep: this.sag?.loadStep };
  }
}
if (typeof module !== 'undefined') module.exports = { AnomalyDetector };
