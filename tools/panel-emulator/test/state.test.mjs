/**
 * test/state.test.mjs
 *
 * Tests for src/state.mjs using Node.js built-in test runner.
 */

import { describe, it, beforeEach } from 'node:test';
import assert from 'node:assert/strict';
import { getState, patchState, resetState, processKeyEvent, DEFAULT_STATE, LAYERS, deepClone } from '../src/state.mjs';

// Reset state before every test so tests are isolated
beforeEach(() => { resetState(); });

// ---------------------------------------------------------------------------
// DEFAULT_STATE
// ---------------------------------------------------------------------------

describe('DEFAULT_STATE', () => {
  it('has the expected top-level keys', () => {
    const keys = Object.keys(DEFAULT_STATE);
    assert.deepEqual(keys.sort(), ['animation','battery','connection','keyboard','power','split'].sort());
  });

  it('has battery at 87% left, 84% right', () => {
    assert.equal(DEFAULT_STATE.battery.left,  87);
    assert.equal(DEFAULT_STATE.battery.right, 84);
  });
});

// ---------------------------------------------------------------------------
// getState / resetState
// ---------------------------------------------------------------------------

describe('getState', () => {
  it('returns a deep clone (not the same reference)', () => {
    const s1 = getState();
    const s2 = getState();
    assert.notStrictEqual(s1, s2);
    assert.deepEqual(s1, s2);
  });

  it('returns DEFAULT_STATE values after reset', () => {
    patchState({ battery: { left: 0 } });
    resetState();
    assert.equal(getState().battery.left, DEFAULT_STATE.battery.left);
  });
});

// ---------------------------------------------------------------------------
// patchState
// ---------------------------------------------------------------------------

describe('patchState — deep merge', () => {
  it('updates a nested field without destroying siblings', () => {
    patchState({ battery: { left: 50 } });
    const s = getState();
    assert.equal(s.battery.left,     50);
    assert.equal(s.battery.right,    DEFAULT_STATE.battery.right);
    assert.equal(s.battery.charging, DEFAULT_STATE.battery.charging);
  });

  it('updates multiple top-level sections in one call', () => {
    patchState({ battery: { left: 15, right: 22 }, keyboard: { activeLayer: 6 } });
    const s = getState();
    assert.equal(s.battery.left,        15);
    assert.equal(s.battery.right,       22);
    assert.equal(s.keyboard.activeLayer, 6);
  });

  it('returns { applied, warnings }', () => {
    const result = patchState({ battery: { left: 60 } });
    assert.ok('applied'  in result);
    assert.ok('warnings' in result);
    assert.ok(Array.isArray(result.warnings));
  });
});

describe('patchState — validation', () => {
  it('rejects an out-of-range activeLayer and returns a warning', () => {
    const before = getState().keyboard.activeLayer;
    const { warnings } = patchState({ keyboard: { activeLayer: 99 } });
    assert.ok(warnings.length > 0, 'should have a warning');
    assert.equal(getState().keyboard.activeLayer, before, 'state unchanged');
  });

  it('accepts all valid layer IDs (0–6)', () => {
    for (let id = 0; id <= 6; id++) {
      resetState();
      const { warnings } = patchState({ keyboard: { activeLayer: id } });
      assert.equal(warnings.length, 0, `layer ${id} should be valid`);
      assert.equal(getState().keyboard.activeLayer, id);
    }
  });

  it('clamps battery.left out of range and returns a warning', () => {
    const { warnings } = patchState({ battery: { left: 150 } });
    assert.ok(warnings.length > 0);
    assert.equal(getState().battery.left, 100);
  });

  it('clamps battery.right below 0 and returns a warning', () => {
    const { warnings } = patchState({ battery: { right: -5 } });
    assert.ok(warnings.length > 0);
    assert.equal(getState().battery.right, 0);
  });
});

// ---------------------------------------------------------------------------
// LAYERS
// ---------------------------------------------------------------------------

describe('LAYERS', () => {
  it('has exactly 7 layers', () => {
    assert.equal(LAYERS.length, 7);
  });

  it('ids are sequential 0–6', () => {
    for (let i = 0; i < 7; i++) {
      assert.equal(LAYERS[i].id, i);
    }
  });

  it('contains UTILS at id 6', () => {
    const utils = LAYERS.find(l => l.key === 'UTILS');
    assert.ok(utils);
    assert.equal(utils.id, 6);
    assert.equal(utils.name, 'Utils');
  });
});

// ---------------------------------------------------------------------------
// processKeyEvent
// ---------------------------------------------------------------------------

describe('processKeyEvent — tap', () => {
  it('sets lastKey', () => {
    processKeyEvent({ type: 'tap', key: 'A' });
    assert.equal(getState().keyboard.lastKey, 'A');
  });

  it('increments activity', () => {
    const before = getState().keyboard.activity;
    processKeyEvent({ type: 'tap', key: 'Z' });
    assert.ok(getState().keyboard.activity > before);
  });

  it('returns a warning when key is missing', () => {
    const { warnings } = processKeyEvent({ type: 'tap' });
    assert.ok(warnings.length > 0);
  });
});

describe('processKeyEvent — key_down / key_up', () => {
  it('adds key to pressedKeys on key_down', () => {
    processKeyEvent({ type: 'key_down', key: 'A' });
    assert.ok(getState().keyboard.pressedKeys.includes('A'));
  });

  it('removes key from pressedKeys on key_up', () => {
    processKeyEvent({ type: 'key_down', key: 'A' });
    processKeyEvent({ type: 'key_up',   key: 'A' });
    assert.ok(!getState().keyboard.pressedKeys.includes('A'));
  });

  it('does not duplicate keys on repeated key_down', () => {
    processKeyEvent({ type: 'key_down', key: 'A' });
    processKeyEvent({ type: 'key_down', key: 'A' });
    const keys = getState().keyboard.pressedKeys;
    assert.equal(keys.filter(k => k === 'A').length, 1);
  });

  it('sets mod shift=true on key_down Shift', () => {
    processKeyEvent({ type: 'key_down', key: 'Shift' });
    assert.equal(getState().keyboard.mods.shift, true);
  });

  it('clears mod shift=false on key_up Shift', () => {
    processKeyEvent({ type: 'key_down', key: 'Shift' });
    processKeyEvent({ type: 'key_up',   key: 'Shift' });
    assert.equal(getState().keyboard.mods.shift, false);
  });

  it('handles Ctrl, Alt, GUI the same way', () => {
    for (const [key, mod] of [['ctrl','ctrl'],['alt','alt'],['gui','gui']]) {
      resetState();
      processKeyEvent({ type: 'key_down', key });
      assert.equal(getState().keyboard.mods[mod], true, `${key} should set mods.${mod}`);
      processKeyEvent({ type: 'key_up', key });
      assert.equal(getState().keyboard.mods[mod], false, `${key} should clear mods.${mod}`);
    }
  });
});

describe('processKeyEvent — clear', () => {
  it('clears pressedKeys and mods', () => {
    processKeyEvent({ type: 'key_down', key: 'A' });
    processKeyEvent({ type: 'key_down', key: 'Shift' });
    processKeyEvent({ type: 'clear' });
    const kb = getState().keyboard;
    assert.equal(kb.pressedKeys.length, 0);
    assert.equal(kb.mods.shift, false);
  });
});

describe('processKeyEvent — unknown type', () => {
  it('returns a warning for unknown event type', () => {
    const { warnings } = processKeyEvent({ type: 'bogus', key: 'A' });
    assert.ok(warnings.length > 0);
    assert.match(warnings[0], /unknown/i);
  });
});

// ---------------------------------------------------------------------------
// resetState
// ---------------------------------------------------------------------------

describe('resetState', () => {
  it('restores DEFAULT_STATE after patches', () => {
    patchState({ battery: { left: 1, right: 2 }, keyboard: { activeLayer: 5 } });
    processKeyEvent({ type: 'key_down', key: 'A' });
    resetState();
    const s = getState();
    assert.equal(s.battery.left,         DEFAULT_STATE.battery.left);
    assert.equal(s.battery.right,        DEFAULT_STATE.battery.right);
    assert.equal(s.keyboard.activeLayer, DEFAULT_STATE.keyboard.activeLayer);
    assert.equal(s.keyboard.pressedKeys.length, 0);
  });

  it('returns the reset state', () => {
    const s = resetState();
    assert.equal(s.battery.left, DEFAULT_STATE.battery.left);
  });
});
