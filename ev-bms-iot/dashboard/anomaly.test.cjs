const { test } = require('node:test');
const assert = require('node:assert/strict');
const { AnomalyDetector } = require('./anomaly.js');
const sample = (t = 30, v = 12, i = 1) => ({ t, v, i });

test('stable temperature and small sensor noise do not trigger', () => {
  const detector = new AnomalyDetector();
  for (let n = 0; n < 30; n++) {
    const result = detector.update(sample(30 + (n % 2) * 0.1), n * 1000);
    assert.equal(result.thermal, false);
    assert.equal(result.sag, false);
  }
});
test('rapid warming triggers before 45 C after three confirmations and clears on cooling', () => {
  const detector = new AnomalyDetector();
  for (let n = 0; n <= 7; n++) {
    const result = detector.update(sample(30 + n * 0.2), n * 1000);
    assert.equal(result.thermal, n >= 7);
    if (n >= 5) assert.ok(Math.abs(result.rate - 12) < 0.001);
  }
  assert.equal(detector.update(sample(29), 8000).thermal, false);
});
test('warming at 3 C/min remains below demo threshold', () => {
  const detector = new AnomalyDetector();
  for (let n = 0; n < 30; n++) assert.equal(detector.update(sample(30 + n * 0.05), n * 1000).thermal, false);
});
test('sag requires both voltage drop and increased load, including discharge', () => {
  for (const sign of [1, -1]) {
    const detector = new AnomalyDetector();
    detector.update(sample(30, 12, sign), 0);
    const result = detector.update(sample(30, 11.5, sign * 4), 1000);
    assert.equal(result.sag, true);
    assert.equal(result.drop, 0.5);
    assert.equal(result.loadStep, 3);
    for (let n = 2; n <= 11; n++) {
      assert.equal(detector.update(sample(30, 11.5, sign * 4), n * 1000).sag, n < 11);
    }
  }
  for (const next of [sample(30, 11.5, 1), sample(30, 12, 4), sample(30, 11.5, -4)]) {
    const detector = new AnomalyDetector();
    detector.update(sample(), 0);
    assert.equal(detector.update(next, 1000).sag, false);
  }
});
test('gaps, invalid input, duplicate timestamps and explicit reset discard baselines', () => {
  for (const action of ['gap', 'invalid', 'duplicate', 'reset']) {
    const detector = new AnomalyDetector();
    detector.update(sample(), 1000);
    let now = 2000;
    if (action === 'gap') now = 7000;
    if (action === 'invalid') assert.equal(detector.update(sample(NaN), 1500).invalid, true);
    if (action === 'duplicate') now = 1000;
    if (action === 'reset') detector.reset();
    const result = detector.update(sample(40, 11, 5), now);
    assert.equal(result.ready, false);
    assert.equal(result.thermal, false);
    assert.equal(result.sag, false);
  }
});
