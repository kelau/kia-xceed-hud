const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const source = fs.readFileSync(path.join(__dirname, '..', 'firmware', 'KiaXCeedHUD', 'web', 'app.js'), 'utf8');
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

console.log(`Verified ${required.length} standard widget metrics and template restoration.`);
