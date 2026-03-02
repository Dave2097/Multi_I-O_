var isSetupPage = document.getElementById('scanBtn') !== null;
var ws = null;
var wsOnline = false;

function api(path, method, body) {
  return fetch(path, {
    method: method || 'GET',
    headers: { 'Content-Type': 'application/json' },
    body: body ? JSON.stringify(body) : undefined
  }).then(function (res) {
    if (!res.ok) {
      return res.text().then(function (t) { throw new Error(t); });
    }
    return res.json();
  });
}

function setStatus(msg, isError) {
  var log = document.getElementById('log');
  if (!log) return;
  log.style.color = isError ? '#ff8f8f' : '#27c07d';
  log.textContent = msg;
}

function setStatus(msg, isError = false) {
  const log = document.getElementById('log');
  if (!log) return;
  log.style.color = isError ? '#ff8f8f' : '#27c07d';
  log.textContent = msg;
}

function renderState(state) {
  var r1 = document.getElementById('relay1');
  var r2 = document.getElementById('relay2');
  if (r1) r1.checked = !!state.relays['1'];
  if (r2) r2.checked = !!state.relays['2'];

  var inputs = document.getElementById('inputs');
  if (inputs) {
    inputs.innerHTML = '';
    var arr = state.inputs || [];
    for (var i = 0; i < arr.length; i++) {
      var it = arr[i];
      var el = document.createElement('div');
      el.className = 'input-chip';
      el.innerHTML = '<strong>DI' + it.id + '</strong><br>Status: ' + (it.state ? 'HIGH' : 'LOW') + '<br>Counter: ' + it.counter;
      inputs.appendChild(el);
    }
  }

  var analogInfo = document.getElementById('analogInfo');
  var analogOutWrap = document.getElementById('analogOutWrap');
  if (analogInfo && state.analog) {
    if (state.analog.mode === 'in') {
      var volts = Number(state.analog.volts || 0).toFixed(3);
      analogInfo.textContent = 'IN · raw=' + state.analog.raw + ' · ' + volts + ' V';
      if (analogOutWrap) analogOutWrap.classList.add('hidden');
    } else {
      analogInfo.textContent = 'OUT · driver=' + state.analog.driver + ' · status=' + state.analog.status;
      if (analogOutWrap) analogOutWrap.classList.remove('hidden');
      var slider = document.getElementById('analogOut');
      var val = document.getElementById('analogOutVal');
      if (slider && val) {
        slider.value = state.analog.value || 0;
        val.textContent = slider.value;
      }
    }
  }

  var system = document.getElementById('system');
  if (system) {
    system.textContent = JSON.stringify(state.system || {}, null, 2);
  }
}

function initDashboard() {
  var relay1 = document.getElementById('relay1');
  var relay2 = document.getElementById('relay2');
  var analogOut = document.getElementById('analogOut');

  if (relay1) {
    relay1.addEventListener('change', function (e) {
      api('/api/relay/1', 'POST', { state: e.target.checked ? 1 : 0 }).catch(function () {});
    });
  }
  if (relay2) {
    relay2.addEventListener('change', function (e) {
      api('/api/relay/2', 'POST', { state: e.target.checked ? 1 : 0 }).catch(function () {});
    });
  }

  var analogDebounce = null;
  if (analogOut) {
    analogOut.addEventListener('input', function (e) {
      var value = Number(e.target.value);
      var outVal = document.getElementById('analogOutVal');
      if (outVal) outVal.textContent = String(value);
      clearTimeout(analogDebounce);
      analogDebounce = setTimeout(function () {
        api('/api/analog/out', 'POST', { value: value }).catch(function () {});
      }, 120);
    });
  }

  var proto = location.protocol === 'https:' ? 'wss' : 'ws';
  ws = new WebSocket(proto + '://' + location.hostname + ':81/');
  ws.onopen = function () { wsOnline = true; };
  ws.onclose = function () { wsOnline = false; };
  ws.onmessage = function (evt) {
    try { renderState(JSON.parse(evt.data)); } catch (e) {}
  };

  setInterval(function () {
    if (!wsOnline) {
      api('/api/state').then(renderState).catch(function () {});
    }
  }, 1000);

  api('/api/state').then(renderState).catch(function () {});
}

function renderScanNetworks(networks) {
  var list = document.getElementById('scanList');
  if (!list) return;
  list.innerHTML = '';

  if (!networks || !networks.length) {
    var empty = document.createElement('li');
    empty.textContent = 'Noch keine Netze gefunden – bitte in 2-3 Sekunden erneut scannen.';
    list.appendChild(empty);
    return;
  }

  for (var i = 0; i < networks.length; i++) {
    (function (n) {
      var li = document.createElement('li');
      var ssid = n.ssid || '(versteckt)';
      li.textContent = ssid + ' · ' + n.rssi + ' dBm';
      li.onclick = function () {
        var ssidInput = document.getElementById('ssid');
        if (ssidInput) ssidInput.value = n.ssid || '';
        setStatus('SSID gewählt: ' + ssid, false);
      };
      list.appendChild(li);
    })(networks[i]);
  }
}

function triggerScanWithRetry() {
  return api('/api/wifi/scan').then(function () {
    var tries = 0;
    function poll() {
      return new Promise(function (resolve) { setTimeout(resolve, 900); })
        .then(function () { return api('/api/wifi/scan'); })
        .then(function (data) {
          var nets = data.networks || [];
          if (nets.length > 0 || tries >= 4) return data;
          tries += 1;
          return poll();
        });
    }
    return poll();
  });
}

function loadCurrentNetworkConfig() {
  return api('/api/config/network').then(function (cfg) {
    var dhcp = document.getElementById('dhcp');
    document.getElementById('ssid').value = cfg.ssid || '';
    dhcp.checked = cfg.dhcp !== false;
    document.getElementById('ip').value = cfg.ip || '192.168.1.200';
    document.getElementById('gw').value = cfg.gw || '192.168.1.1';
    document.getElementById('mask').value = cfg.mask || '255.255.255.0';
    document.getElementById('dns').value = cfg.dns || '8.8.8.8';
    document.getElementById('staticFields').classList.toggle('hidden', dhcp.checked);
  }).catch(function (e) {
    setStatus('Konnte vorhandene Netzwerte nicht laden: ' + e.message, true);
  });
}

function initSetup() {
  var scanBtn = document.getElementById('scanBtn');
  var saveBtn = document.getElementById('saveBtn');
  var dhcp = document.getElementById('dhcp');
  var staticFields = document.getElementById('staticFields');

  dhcp.addEventListener('change', function () {
    staticFields.classList.toggle('hidden', dhcp.checked);
  });

  scanBtn.addEventListener('click', function () {
    scanBtn.disabled = true;
    setStatus('Suche WLAN-Netze...', false);
    triggerScanWithRetry().then(function (data) {
      var nets = data.networks || [];
      renderScanNetworks(nets);
      setStatus('Scan abgeschlossen: ' + nets.length + ' Netz(e) gefunden.', false);
    }).catch(function (e) {
      setStatus('Scan Fehler: ' + e.message, true);
    }).then(function () {
      scanBtn.disabled = false;
    });
  });

  saveBtn.addEventListener('click', function () {
    saveBtn.disabled = true;
    setStatus('Speichere Netzwerkdaten...', false);

    var body = {
      ssid: document.getElementById('ssid').value.trim(),
      password: document.getElementById('password').value,
      dhcp: dhcp.checked,
      ip: document.getElementById('ip').value.trim(),
      gw: document.getElementById('gw').value.trim(),
      mask: document.getElementById('mask').value.trim(),
      dns: document.getElementById('dns').value.trim()
    };

    if (!body.ssid) {
      setStatus('Save Fehler: SSID darf nicht leer sein', true);
      saveBtn.disabled = false;
      return;
    }

    api('/api/config/network', 'POST', body).then(function () {
      setStatus('Gespeichert. Gerät startet neu...', false);
    }).catch(function (e) {
      setStatus('Save Fehler: ' + e.message, true);
      saveBtn.disabled = false;
    });
  });

  loadCurrentNetworkConfig().then(function () {
    setStatus('Bereit. Bitte WLAN auswählen und speichern.', false);
  });

  await loadCurrentNetworkConfig();
  setStatus('Bereit. Bitte WLAN auswählen und speichern.');
}

if (isSetupPage) {
  initSetup();
} else {
  initDashboard();
}
