"use strict";

const state = {
  authenticated: false,
  status: null,
  socket: null,
  heartbeat: null,
  pointerId: null,
  action: 0,
  sequence: 0,
  polling: false,
};

const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => Array.from(document.querySelectorAll(selector));
const loginDialog = $("#login-dialog");
const movementButtons = $$(".hold-control");

async function request(path, options = {}) {
  const response = await fetch(path, {
    credentials: "same-origin",
    cache: "no-store",
    ...options,
  });
  let payload = {};
  try {
    payload = await response.json();
  } catch (_) {
    payload = {};
  }
  if (!response.ok) {
    const error = new Error(payload.error || `http_${response.status}`);
    error.status = response.status;
    throw error;
  }
  return payload;
}

function titleCase(value) {
  return value.replaceAll("_", " ").replace(/\b\w/g, (letter) => letter.toUpperCase());
}

function updateAxis(axis, index) {
  const id = ["a", "b", "c"][index];
  const millimeters = axis.position_um / 1000;
  $(`#axis-${id}-position`).textContent = `${millimeters.toFixed(1)} mm`;
  $(`#axis-${id}-level`).style.width =
    `${Math.max(0, Math.min(100, axis.position_um / 5000))}%`;
  $(`#axis-${id}-lower`).classList.toggle("active", axis.lower);
  $(`#axis-${id}-upper`).classList.toggle("active", axis.upper);
}

function updateControls(status) {
  const remoteAllowed = status.mode !== "local";
  const stoppedBySafety = status.emergency || status.error !== "none";
  const canMove = state.authenticated && remoteAllowed && status.referenced &&
    !stoppedBySafety;
  $("#move-up").disabled = !canMove;
  $("#move-down").disabled = !canMove;
  $("#calibrate").disabled = !state.authenticated || !remoteAllowed ||
    status.emergency || status.state === "moving_up" || status.state === "moving_down";
  $$(".preset-motion").forEach((button) => {
    const preset = status.presets[Number(button.dataset.slot)];
    button.disabled = !canMove || !preset?.valid;
  });
  const canCapture = state.authenticated && status.state === "idle" &&
    status.error === "none" && status.referenced && !status.feedback_warning &&
    !status.emergency && !status.remote_active;
  $$(".preset-save").forEach((button) => {
    button.disabled = !canCapture;
  });
  $$(".segments button").forEach((button) => {
    button.classList.toggle("active", button.dataset.mode === status.mode);
    button.disabled = status.remote_active;
  });
}

function renderStatus(status) {
  state.status = status;
  $("#connection-dot").classList.add("online");
  $("#connection-dot").classList.remove("offline");
  $("#connection-label").textContent = status.wifi_connected ? "WiFi" : "Directo";
  $("#desk-state").textContent = titleCase(status.state);
  $("#control-source").textContent = titleCase(status.source);
  $("#mode-label").textContent = titleCase(status.mode);
  const average = status.axes.reduce((sum, axis) => sum + axis.position_um, 0) /
    status.axes.length / 1000;
  $("#desk-height").textContent = status.referenced ? average.toFixed(1) : "--";
  status.axes.forEach(updateAxis);
  status.presets.forEach((preset, index) => {
    $(`#preset-${index + 1}-height`).textContent =
      preset.valid ? `${(preset.target_um / 1000).toFixed(1)} mm` : "--";
  });

  const alert = $("#alert");
  const hasAlert = status.error !== "none" || status.emergency || status.feedback_warning;
  alert.classList.toggle("hidden", !hasAlert);
  if (hasAlert) {
    $("#alert-title").textContent = status.emergency ? "E-stop activo" :
      status.feedback_warning ? "Desnivel" : "Error";
    $("#alert-detail").textContent = status.error === "none" ? "" :
      titleCase(status.error);
  }
  $("#provisioning").classList.toggle("hidden", !status.provisioning);
  updateControls(status);
}

async function pollStatus() {
  if (state.polling) {
    return;
  }
  state.polling = true;
  try {
    const status = await request("/api/v1/status");
    state.authenticated = true;
    renderStatus(status);
    if (loginDialog.open) {
      loginDialog.close();
    }
  } catch (error) {
    $("#connection-dot").classList.remove("online");
    $("#connection-label").textContent = "Offline";
    if (error.status === 401) {
      state.authenticated = false;
      stopMotion();
      movementButtons.forEach((button) => {
        button.disabled = true;
      });
      if (!loginDialog.open) {
        loginDialog.showModal();
      }
    }
  } finally {
    state.polling = false;
  }
}

function commandFrame(action) {
  const buffer = new ArrayBuffer(12);
  const view = new DataView(buffer);
  view.setUint8(0, 1);
  view.setUint8(1, 1);
  view.setUint8(2, action);
  view.setUint8(3, 0);
  view.setUint32(4, ++state.sequence, false);
  view.setUint16(8, 250, false);
  view.setUint16(10, 0, false);
  return buffer;
}

function sendCommand(action) {
  if (state.socket?.readyState === WebSocket.OPEN) {
    state.socket.send(commandFrame(action));
  }
}

function stopMotion() {
  if (state.heartbeat !== null) {
    clearInterval(state.heartbeat);
    state.heartbeat = null;
  }
  movementButtons.forEach((button) => button.classList.remove("pressed"));
  if (state.socket?.readyState === WebSocket.OPEN) {
    sendCommand(0);
  }
  state.socket?.close();
  state.socket = null;
  state.pointerId = null;
  state.action = 0;
}

function beginMotion(button, event) {
  if (button.disabled || state.socket !== null || state.pointerId !== null) {
    return;
  }
  event.preventDefault();
  button.setPointerCapture(event.pointerId);
  state.pointerId = event.pointerId;
  state.action = Number(button.dataset.action);
  state.sequence = 0;
  button.classList.add("pressed");

  const scheme = location.protocol === "https:" ? "wss" : "ws";
  const socket = new WebSocket(
    `${scheme}://${location.host}/api/v1/control`,
    "desk-control-v1",
  );
  socket.binaryType = "arraybuffer";
  state.socket = socket;
  socket.addEventListener("open", () => {
    if (state.socket !== socket || state.pointerId === null) {
      socket.close();
      return;
    }
    sendCommand(state.action);
    state.heartbeat = setInterval(() => sendCommand(state.action), 100);
  });
  socket.addEventListener("close", () => {
    if (state.socket === socket) {
      if (state.heartbeat !== null) {
        clearInterval(state.heartbeat);
      }
      state.heartbeat = null;
      state.socket = null;
      state.pointerId = null;
      state.action = 0;
      button.classList.remove("pressed");
    }
  });
  socket.addEventListener("error", stopMotion);
}

movementButtons.forEach((button) => {
  button.addEventListener("pointerdown", (event) => beginMotion(button, event));
  button.addEventListener("pointerup", stopMotion);
  button.addEventListener("pointercancel", stopMotion);
  button.addEventListener("lostpointercapture", stopMotion);
  button.addEventListener("contextmenu", (event) => event.preventDefault());
});

$("#login-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);
  try {
    await request("/api/v1/session", {
      method: "POST",
      headers: {"Content-Type": "application/x-www-form-urlencoded"},
      body: new URLSearchParams({token: form.get("token")}),
    });
    $("#login-error").textContent = "";
    event.currentTarget.reset();
    await pollStatus();
  } catch (error) {
    $("#login-error").textContent = titleCase(error.message);
  }
});

$("#logout").addEventListener("click", async () => {
  stopMotion();
  try {
    await request("/api/v1/session", {method: "DELETE"});
  } finally {
    state.authenticated = false;
    if (!loginDialog.open) {
      loginDialog.showModal();
    }
  }
});

$$(".segments button").forEach((button) => {
  button.addEventListener("click", async () => {
    stopMotion();
    try {
      await request("/api/v1/mode", {
        method: "POST",
        headers: {"Content-Type": "application/x-www-form-urlencoded"},
        body: new URLSearchParams({mode: button.dataset.mode}),
      });
      await pollStatus();
    } catch (_) {
      await pollStatus();
    }
  });
});

$$(".preset-save").forEach((button) => {
  button.addEventListener("click", async () => {
    stopMotion();
    try {
      await request("/api/v1/preset", {
        method: "POST",
        headers: {"Content-Type": "application/x-www-form-urlencoded"},
        body: new URLSearchParams({slot: String(Number(button.dataset.slot) + 1)}),
      });
    } catch (_) {
      // The following status poll exposes the authoritative safety state.
    } finally {
      await pollStatus();
    }
  });
});

$("#wifi-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  stopMotion();
  const form = new FormData(event.currentTarget);
  try {
    await request("/api/v1/provision", {
      method: "POST",
      headers: {"Content-Type": "application/x-www-form-urlencoded"},
      body: new URLSearchParams({
        ssid: form.get("ssid"),
        password: form.get("password"),
      }),
    });
    event.currentTarget.reset();
  } catch (_) {
    await pollStatus();
  }
});

window.addEventListener("blur", stopMotion);
window.addEventListener("pagehide", stopMotion);
document.addEventListener("visibilitychange", () => {
  if (document.hidden) {
    stopMotion();
  }
});

pollStatus();
setInterval(pollStatus, 500);
