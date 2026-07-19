const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const source = fs.readFileSync(path.join(__dirname, '..', 'firmware', 'KiaXCeedHUD', 'web', 'app.js'), 'utf8');
const features = fs.readFileSync(path.join(__dirname, '..', 'firmware', 'KiaXCeedHUD', 'web', 'features.js'), 'utf8');
const runtime = fs.readFileSync(path.join(__dirname, '..', 'firmware', 'KiaXCeedHUD', 'web', 'runtime.js'), 'utf8');
const analysis = fs.readFileSync(path.join(__dirname, '..', 'firmware', 'KiaXCeedHUD', 'web', 'analysis.js'), 'utf8');
const dashboard = fs.readFileSync(path.join(__dirname, '..', 'firmware', 'KiaXCeedHUD', 'src', 'Dashboard.h'), 'utf8');
const webSources = ['index.html', 'app.js', 'analysis.js', 'features.js', 'runtime.js']
  .map(name => fs.readFileSync(path.join(__dirname, '..', 'firmware', 'KiaXCeedHUD', 'web', name), 'utf8'));
const required = [
  'speed', 'rpm', 'power', 'soc', 'load', 'coolant', 'intakeTemp',
  'throttle', 'voltage', 'ambientTemp', 'fuelRate', 'trip', 'gpsSpeed',
  'gpsLock', 'coordinates', 'accel', 'status', 'canAge', 'mode', 'wifi',
  'wifiSignal', 'version', 'uptime', 'time', 'date', 'webAccess'
];

for (const id of required) {
  assert.match(source, new RegExp(`['"]${id}['"]`), `widget catalog is missing ${id}`);
}
assert.match(source, /widgets\.find\(w=>w\.id===id\)\|\|widgetTemplate\(id\)/,
  'palette must provide a template when a display has no persisted metric widget');
assert.match(source, /if\(!existing\)widgets\.push\(w\)/,
  'dropping a missing metric must add a new widget to the selected display');
assert.match(source, /widgets:configData\.widgets/,
  'saving a virtual display must not replace the main display widget array');
assert.match(features, /function storeDisplay\(\).*d\.widgets=widgets/,
  'the selected virtual display must be stored before saving');
assert.match(features, /\/hud\/graphics/,
  'graphic resources must use the dedicated SD card folder');
assert.match(runtime, /page === "status"/,
  'stream updates must be gated by the visible page');
assert.match(source, /function drawGraphic\(/,
  'the editor must render Graphic widgets');
assert.match(source, /new Uint8Array\(raw\)/,
  'the editor must decode SD-card RGB565 resources');
assert.doesNotMatch(dashboard, /if\(!usesMetricGlyphs\(w\)\)return w\.fontSize/,
  'native large fonts must also be used by time, date, status, and other text widgets');
assert.match(analysis, /HUD_REFRESH_SELECTED_FRAME/,
  'frame events must explicitly refresh the selected frame detail');
assert.match(runtime, /HUD_REFRESH_SELECTED_FRAME/,
  'the event stream must invoke the selected-frame refresh callback');
for (const text of webSources) {
  assert.doesNotMatch(text, /Â|â|Ã/, 'Web UI source contains mojibake');
}

console.log(`Verified ${required.length} standard widget metrics and template restoration.`);
