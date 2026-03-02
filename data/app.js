const isSetupPage = location.pathname.startsWith('/setup');
let ws = null;
let wsOnline = false;

async function api(path, method = 'GET', body) {
  const res = await fetch(path, {
    method,
    headers: { 'Content-Type': 'application/json' },
    body: body ? JSON.stringify(body) : undefined
  });
  if (!res.ok) {
    throw new Error(await res.text());
  }
  return await res.json();
}

function setStatus(msg, isError = false) {
  const log = document.getElementById('log');
  if (!log) return;
  log.style.color = isError ? '#ff8f8f' : '#27c07d';
  log.textContent = msg;
}

function renderState(state) {
  const r1 = document.getElementById('relay1');
  const r2 = document.getElementById('relay2');
  if (r1) r1.checked = !!state.relays['1'];
  if (r2) r2.checked = !!state.relays['2'];

  const inputs = document.getElementById('inputs');
  if (inputs) {
    inputs.innerHTML = '';
    (state.inputs || []).forEach(i => {
      const el = document.createElement('div');
      el.className = 'input-chip';
      el.innerHTML = `<strong>DI${i.id}</strong><br>Status: ${i.state ? 'HIGH' : 'LOW'}<br>Counter: ${i.counter}`;
      inputs.appendChild(el);
    });
  }

  const analogInfo = document.getElementById('analogInfo');
  const analogOutWrap = document.getElementById('analogOutWrap');
  if (analogInfo) {
    if (state.analog.mode === 'in') {
      analogInfo.textContent = `IN · raw=${state.analog.raw} · ${Number(state.analog.volts).toFixed(3)} V`;
      analogOutWrap?.classList.add('hidden');
    } else {
      analogInfo.textContent = `OUT · driver=${state.analog.driver} · status=${state.analog.status}`;
      analogOutWrap?.classList.remove('hidden');
      const slider = document.getElementById('analogOut');
      const val = document.getElementById('analogOutVal');
      if (slider && val) {
        slider.value = state.analog.value ?? 0;
        val.textContent = slider.value;
      }
    }
  }

  const system = document.getElementById('system');
  if (system) {
    system.textContent = JSON.stringify(state.system, null, 2);
  }
}

async function initDashboard() {
  document.getElementById('relay1')?.addEventListener('change', async (e) => {
    await api('/api/relay/1', 'POST', { state: e.target.checked ? 1 : 0 });
  });
  document.getElementById('relay2')?.addEventListener('change', async (e) => {
    await api('/api/relay/2', 'POST', { state: e.target.checked ? 1 : 0 });
  });

  let analogDebounce;
  document.getElementById('analogOut')?.addEventListener('input', (e) => {
    const value = Number(e.target.value);
    document.getElementById('analogOutVal').textContent = value;
    clearTimeout(analogDebounce);
    analogDebounce = setTimeout(async () => {
      try { await api('/api/analog/out', 'POST', { value }); } catch (_) {}
    }, 120);
  });

  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  ws = new WebSocket(`${proto}://${location.hostname}:81/`);
  ws.onopen = () => { wsOnline = true; };
  ws.onclose = () => { wsOnline = false; };
  ws.onmessage = (evt) => {
    try { renderState(JSON.parse(evt.data)); } catch (_) {}
  };

  setInterval(async () => {
    if (!wsOnline) {
      try {
        const state = await api('/api/state');
        renderState(state);
      } catch (_) {}
    }
  }, 1000);

  renderState(await api('/api/state'));
}

function renderScanNetworks(networks) {
  const list = document.getElementById('scanList');
  if (!list) return;
  list.innerHTML = '';

  if (!networks?.length) {
    const li = document.createElement('li');
    li.textContent = 'Noch keine Netze gefunden – bitte in 2-3 Sekunden erneut scannen.';
    list.appendChild(li);
    return;
  }

  networks.forEach(n => {
    const li = document.createElement('li');
    li.textContent = `${n.ssid || '(versteckt)'} · ${n.rssi} dBm`;
    li.onclick = () => {
      document.getElementById('ssid').value = n.ssid || '';
      setStatus(`SSID gewählt: ${n.ssid || '(versteckt)'}`);
    };
    list.appendChild(li);
  });
}

async function triggerScanWithRetry() {
  // 1st call often only starts async scan on ESP8266, so poll shortly after
  await api('/api/wifi/scan');
  for (let i = 0; i < 4; i++) {
    await new Promise(r => setTimeout(r, 900));
    const data = await api('/api/wifi/scan');
    if ((data.networks || []).length > 0) {
      return data;
    }
  }
  return await api('/api/wifi/scan');
}

async function loadCurrentNetworkConfig() {
  try {
    const cfg = await api('/api/config/network');
    document.getElementById('ssid').value = cfg.ssid || '';
    document.getElementById('dhcp').checked = cfg.dhcp !== false;
    document.getElementById('ip').value = cfg.ip || '192.168.1.200';
    document.getElementById('gw').value = cfg.gw || '192.168.1.1';
    document.getElementById('mask').value = cfg.mask || '255.255.255.0';
    document.getElementById('dns').value = cfg.dns || '8.8.8.8';
    document.getElementById('staticFields').classList.toggle('hidden', document.getElementById('dhcp').checked);
  } catch (e) {
    setStatus(`Konnte vorhandene Netzwerte nicht laden: ${e.message}`, true);
  }
}

async function initSetup() {
  const scanBtn = document.getElementById('scanBtn');
  const saveBtn = document.getElementById('saveBtn');
  const dhcp = document.getElementById('dhcp');
  const staticFields = document.getElementById('staticFields');

  dhcp.addEventListener('change', () => {
    staticFields.classList.toggle('hidden', dhcp.checked);
  });

  scanBtn.addEventListener('click', async () => {
    scanBtn.disabled = true;
    setStatus('Suche WLAN-Netze...');
    try {
      const data = await triggerScanWithRetry();
      renderScanNetworks(data.networks || []);
      setStatus(`Scan abgeschlossen: ${(data.networks || []).length} Netz(e) gefunden.`);
    } catch (e) {
      setStatus(`Scan Fehler: ${e.message}`, true);
    } finally {
      scanBtn.disabled = false;
    }
  });

  saveBtn.addEventListener('click', async () => {
    saveBtn.disabled = true;
    setStatus('Speichere Netzwerkdaten...');
    try {
      const body = {
        ssid: document.getElementById('ssid').value.trim(),
        password: document.getElementById('password').value,
        dhcp: dhcp.checked,
        ip: document.getElementById('ip').value.trim(),
        gw: document.getElementById('gw').value.trim(),
        mask: document.getElementById('mask').value.trim(),
        dns: document.getElementById('dns').value.trim()
      };

      if (!body.ssid) {
        throw new Error('SSID darf nicht leer sein');
      }

      await api('/api/config/network', 'POST', body);
      setStatus('Gespeichert. Gerät startet neu...');
    } catch (e) {
      setStatus(`Save Fehler: ${e.message}`, true);
      saveBtn.disabled = false;
    }
  });

  await loadCurrentNetworkConfig();
  setStatus('Bereit. Bitte WLAN auswählen und speichern.');
}

if (isSetupPage) {
  initSetup();
} else {
  initDashboard();
}
