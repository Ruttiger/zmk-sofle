/**
 * state.mjs
 *
 * Keyboard state simulation for the OLED panel emulator.
 * Manages keyboard state: battery, connection, split, keyboard input, power, animation.
 *
 * @module state
 */

// ---------------------------------------------------------------------------
// Layers
// ---------------------------------------------------------------------------

export const LAYERS = [
  { id: 0, key: 'WINDOWS',    name: 'Windows'    },
  { id: 1, key: 'WINDOWS_FN', name: 'Win Fn'     },
  { id: 2, key: 'MAC',        name: 'Mac'         },
  { id: 3, key: 'MAC_FN',     name: 'Mac Fn'      },
  { id: 4, key: 'GALLIUM',    name: 'Gallium'     },
  { id: 5, key: 'GALLIUM_FN', name: 'Gallium Fn'  },
  { id: 6, key: 'UTILS',      name: 'Utils'       },
];

// ---------------------------------------------------------------------------
// Default state
// ---------------------------------------------------------------------------

export const DEFAULT_STATE = Object.freeze({
  battery: {
    left:     87,
    right:    84,
    charging: false,
  },
  connection: {
    output:     'BLE',
    usbConnected: false,
    bleConnected: true,
    bleProfile:   0,
    bleProfiles: [
      { id: 0, bonded: true,  connected: true  },
      { id: 1, bonded: false, connected: false },
      { id: 2, bonded: false, connected: false },
      { id: 3, bonded: false, connected: false },
      { id: 4, bonded: false, connected: false },
    ],
    advertising: false,
    pairing:     false,
  },
  split: {
    leftConnected:  true,
    rightConnected: true,
    linkQuality:    'ok',
  },
  keyboard: {
    activeLayer: 0,
    pressedKeys: [],
    lastKey:     null,
    mods: {
      ctrl:  false,
      shift: false,
      alt:   false,
      gui:   false,
    },
    locks: {
      caps:   false,
      num:    false,
      scroll: false,
    },
    wpm:      0,
    activity: 0,
  },
  power: {
    idle:    false,
    sleep:   false,
    softOff: false,
  },
  animation: {
    catEnabled: true,
    catSpeed:   1,
    frame:      0,
  },
});

// ---------------------------------------------------------------------------
// Mutable state
// ---------------------------------------------------------------------------

let _state = deepClone(DEFAULT_STATE);

// ---------------------------------------------------------------------------
// Public getters/setters
// ---------------------------------------------------------------------------

/**
 * Return a deep clone of the current state.
 * @returns {typeof DEFAULT_STATE}
 */
export function getState() {
  return deepClone(_state);
}

/**
 * Apply a partial deep-merge update to the state.
 * Returns { applied: Object, warnings: string[] }.
 *
 * @param {Partial<typeof DEFAULT_STATE>} partial
 * @returns {{ applied: Object, warnings: string[] }}
 */
export function patchState(partial) {
  const warnings = [];

  // Validate before applying
  if (partial.keyboard?.activeLayer !== undefined) {
    const layer = partial.keyboard.activeLayer;
    if (!Number.isInteger(layer) || layer < 0 || layer >= LAYERS.length) {
      warnings.push(
        `activeLayer ${layer} is out of range (0–${LAYERS.length - 1}). State unchanged for this field.`
      );
      // Remove invalid field to avoid applying it
      partial = deepClone(partial);
      delete partial.keyboard.activeLayer;
    }
  }

  if (partial.battery?.left !== undefined) {
    const v = partial.battery.left;
    if (v < 0 || v > 100) {
      warnings.push(`battery.left ${v} out of range (0–100). Clamped.`);
      partial = deepClone(partial);
      partial.battery.left = Math.max(0, Math.min(100, v));
    }
  }

  if (partial.battery?.right !== undefined) {
    const v = partial.battery.right;
    if (v < 0 || v > 100) {
      warnings.push(`battery.right ${v} out of range (0–100). Clamped.`);
      partial = deepClone(partial);
      partial.battery.right = Math.max(0, Math.min(100, v));
    }
  }

  _state = deepMerge(_state, partial);
  return { applied: deepClone(partial), warnings };
}

/**
 * Reset state to DEFAULT_STATE.
 * @returns {typeof DEFAULT_STATE}
 */
export function resetState() {
  _state = deepClone(DEFAULT_STATE);
  return getState();
}

// ---------------------------------------------------------------------------
// Key events
// ---------------------------------------------------------------------------

const MOD_KEYS = new Set(['ctrl', 'shift', 'alt', 'gui',
                          'lctrl', 'rctrl', 'lshift', 'rshift',
                          'lalt', 'ralt', 'lgui', 'rgui']);

const MOD_MAP = {
  ctrl: 'ctrl', lctrl: 'ctrl', rctrl: 'ctrl',
  shift: 'shift', lshift: 'shift', rshift: 'shift',
  alt: 'alt', lalt: 'alt', ralt: 'alt',
  gui: 'gui', lgui: 'gui', rgui: 'gui',
};

const WPM_UPDATE_WEIGHT = 0.2; // exponential moving average weight

/**
 * Process a key event and update keyboard state.
 *
 * @param {{ type: 'tap'|'key_down'|'key_up'|'clear', key?: string }} event
 * @returns {{ warnings: string[] }}
 */
export function processKeyEvent(event) {
  const warnings = [];
  const { type, key } = event;
  const kb = _state.keyboard;

  switch (type) {
    case 'tap': {
      if (!key) { warnings.push('key_event tap: missing "key" field.'); break; }
      kb.lastKey = key;
      kb.activity = Math.min(100, kb.activity + 5);
      // Simple WPM bump (1 keypress ≈ 0.2 of a word)
      kb.wpm = Math.round(kb.wpm * (1 - WPM_UPDATE_WEIGHT) + 60 * WPM_UPDATE_WEIGHT);
      break;
    }

    case 'key_down': {
      if (!key) { warnings.push('key_event key_down: missing "key" field.'); break; }
      const keyLower = key.toLowerCase();
      const modName  = MOD_MAP[keyLower];
      if (modName) {
        kb.mods[modName] = true;
      } else {
        if (!kb.pressedKeys.includes(key)) {
          kb.pressedKeys = [...kb.pressedKeys, key];
        }
        kb.lastKey = key;
      }
      break;
    }

    case 'key_up': {
      if (!key) { warnings.push('key_event key_up: missing "key" field.'); break; }
      const keyLower = key.toLowerCase();
      const modName  = MOD_MAP[keyLower];
      if (modName) {
        kb.mods[modName] = false;
      } else {
        kb.pressedKeys = kb.pressedKeys.filter(k => k !== key);
      }
      break;
    }

    case 'clear': {
      kb.pressedKeys = [];
      kb.mods = { ctrl: false, shift: false, alt: false, gui: false };
      break;
    }

    default:
      warnings.push(`Unknown key event type "${type}".`);
  }

  return { warnings };
}

// ---------------------------------------------------------------------------
// Deep utilities (no external deps)
// ---------------------------------------------------------------------------

/**
 * Deep clone a plain object/array/primitive (no class instances, no functions).
 * @param {*} val
 * @returns {*}
 */
export function deepClone(val) {
  if (val === null || typeof val !== 'object') return val;
  if (Array.isArray(val)) return val.map(deepClone);
  const out = {};
  for (const [k, v] of Object.entries(val)) out[k] = deepClone(v);
  return out;
}

/**
 * Deep merge `patch` into `base`. Returns a new object; does not mutate inputs.
 * Arrays in `patch` overwrite arrays in `base` entirely.
 *
 * @param {Object} base
 * @param {Object} patch
 * @returns {Object}
 */
function deepMerge(base, patch) {
  if (patch === null || typeof patch !== 'object' || Array.isArray(patch)) return patch;
  const out = deepClone(base);
  for (const [k, v] of Object.entries(patch)) {
    if (v !== null && typeof v === 'object' && !Array.isArray(v) &&
        out[k] !== null && typeof out[k] === 'object' && !Array.isArray(out[k])) {
      out[k] = deepMerge(out[k], v);
    } else {
      out[k] = deepClone(v);
    }
  }
  return out;
}
