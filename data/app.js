const isSetupPage = location.pathname.includes('/setup');
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

function renderState(state) {
  const r1 = document.getElementById('relay1');
  const r2 = document.getElementById('relay2');
  if (r1) r1.checked = !!state.relays['1'];
  if (r2) r2.checked = !!state.relays['2'];

  const inputs = document.getElementById('inputs');
  if (inputs) {
    inputs.innerHTML = state.inputs
      .map(i => `DI${i.id}: ${i.state ? 'HIGH' : 'LOW'} | counter=${i.counter}`)
      .join('<br>');
  }

  const analogInfo = document.getElementById('analogInfo');
  const analogOutWrap = document.getElementById('analogOutWrap');
  if (analogInfo) {
    if (state.analog.mode === 'in') {
      analogInfo.textContent = `IN raw=${state.analog.raw}, volts=${Number(state.analog.volts).toFixed(3)}V`;
      analogOutWrap?.classList.add('hidden');
    } else {
      analogInfo.textContent = `OUT driver=${state.analog.driver}, status=${state.analog.status}`;
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

  document.getElementById('analogOut')?.addEventListener('input', async (e) => {
    const value = Number(e.target.value);
    document.getElementById('analogOutVal').textContent = value;
    await api('/api/analog/out', 'POST', { value });
  });

  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  ws = new WebSocket(`${proto}://${location.hostname}:81/`);
  ws.onopen = () => wsOnline = true;
  ws.onclose = () => wsOnline = false;
  ws.onmessage = (evt) => {
    try { renderState(JSON.parse(evt.data)); } catch (e) {}
  };

  setInterval(async () => {
    if (!wsOnline) {
      try {
        const state = await api('/api/state');
        renderState(state);
      } catch (e) {}
    }
  }, 500);

  renderState(await api('/api/state'));
}

async function initSetup() {
  const scanBtn = document.getElementById('scanBtn');
  const saveBtn = document.getElementById('saveBtn');
  const dhcp = document.getElementById('dhcp');
  const staticFields = document.getElementById('staticFields');
  const log = document.getElementById('log');

  dhcp.addEventListener('change', () => {
    staticFields.classList.toggle('hidden', dhcp.checked);
  });

  scanBtn.addEventListener('click', async () => {
    try {
      const data = await api('/api/wifi/scan');
      const list = document.getElementById('scanList');
      list.innerHTML = '';
      (data.networks || []).forEach(n => {
        const li = document.createElement('li');
        li.textContent = `${n.ssid} (${n.rssi} dBm)`;
        li.onclick = () => document.getElementById('ssid').value = n.ssid;
        list.appendChild(li);
      });
    } catch (e) {
      log.textContent = `Scan Fehler: ${e.message}`;
    }
  });

  saveBtn.addEventListener('click', async () => {
    try {
      const body = {
        ssid: document.getElementById('ssid').value,
        password: document.getElementById('password').value,
        dhcp: dhcp.checked,
        ip: document.getElementById('ip').value,
        gw: document.getElementById('gw').value,
        mask: document.getElementById('mask').value,
        dns: document.getElementById('dns').value
      };
      await api('/api/config/network', 'POST', body);
      log.textContent = 'Gespeichert, Neustart...';
    } catch (e) {
      log.textContent = `Save Fehler: ${e.message}`;
    }
  });
}

if (isSetupPage) {
  initSetup();
} else {
  initDashboard();
}
