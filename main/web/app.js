"use strict";

const STATUS_POLL_MS = 750;
const REQUEST_TIMEOUT_MS = 4000;
const SOCKET_CONNECT_TIMEOUT_MS = 1500;
const HEARTBEAT_MS = 100;

const state = {
  authenticated: false,
  authPromptDismissed: false,
  status: null,
  socket: null,
  socketConnectTimer: null,
  heartbeat: null,
  pointerId: null,
  action: 0,
  sequence: 0,
  polling: false,
  pollTimer: null,
  confirmResolver: null,
};

const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => Array.from(document.querySelectorAll(selector));
const loginDialog = $("#login-dialog");
const confirmDialog = $("#confirm-dialog");
const movementButtons = $$(".hold-control");

function humanize(value) {
  return String(value || "unknown")
    .replaceAll("_", " ")
    .replace(/\b\w/g, (letter) => letter.toUpperCase());
}

function showToast(message, isError = false) {
  const toast = document.createElement("div");
  toast.className = `toast${isError ? " error" : ""}`;
  toast.textContent = message;
  $("#toast-region").append(toast);
  setTimeout(() => toast.remove(), 4200);
}

function setIndicator(kind, label, cssState) {
  $(`#${kind}-status`).textContent = label;
  $(`#${kind}-dot`).className = `status-dot ${cssState}`;
}

async function request(path, options = {}) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);
  try {
    const response = await fetch(path, {
      credentials: "same-origin",
      cache: "no-store",
      headers: {Accept: "application/json", ...(options.headers || {})},
      ...options,
      signal: controller.signal,
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
  } catch (error) {
    if (error.name === "AbortError") {
      throw new Error("request_timeout");
    }
    throw error;
  } finally {
    clearTimeout(timeout);
  }
}

function openLogin(force = false) {
  if (loginDialog.open || (!force && state.authPromptDismissed)) {
    return;
  }
  loginDialog.showModal();
  requestAnimationFrame(() => $("#login-form input[name='token']").focus());
}

function closeLogin() {
  state.authPromptDismissed = true;
  $("#login-error").textContent = "";
  if (loginDialog.open) {
    loginDialog.close();
  }
}

function askConfirmation(title, detail, acceptLabel = "Confirm") {
  if (state.confirmResolver !== null) {
    state.confirmResolver(false);
  }
  $("#confirm-title").textContent = title;
  $("#confirm-detail").textContent = detail;
  $("#confirm-accept").textContent = acceptLabel;
  confirmDialog.showModal();
  return new Promise((resolve) => {
    state.confirmResolver = resolve;
  });
}

function resolveConfirmation(accepted) {
  if (confirmDialog.open) {
    confirmDialog.close();
  }
  const resolver = state.confirmResolver;
  state.confirmResolver = null;
  if (resolver !== null) {
    resolver(accepted);
  }
}

function updateAxis(axis, index) {
  const id = ["a", "b", "c"][index];
  const millimeters = axis.position_um / 1000;
  $(`#axis-${id}-position`).textContent = `${millimeters.toFixed(1)} mm`;
  $(`#axis-${id}-level`).style.width =
    `${Math.max(0, Math.min(100, axis.position_um / 5000))}%`;
  $(`#axis-${id}-lower`).classList.toggle("active", axis.lower);
  $(`#axis-${id}-upper`).classList.toggle("active", axis.upper);
  $(`#axis-${id}-hall`).textContent = axis.hall_valid ? `Hall ${axis.hall}` : "Hall fault";
}

function movementReason(status) {
  if (!state.authenticated) {
    return "Sign in to enable remote control.";
  }
  if (status.mode === "local") {
    return "Remote motion is disabled in Local only mode.";
  }
  if (status.emergency) {
    return "Release the physical emergency stop and clear the latched error.";
  }
  if (status.error !== "none") {
    return `Motion is inhibited: ${humanize(status.error)}.`;
  }
  if (!status.referenced) {
    return "Calibrate the desk before normal movement or presets.";
  }
  if (status.motion_inhibited) {
    return "Motion is inhibited while maintenance is active.";
  }
  return "Ready. Keep a control held for the entire movement.";
}

function updateControls(status) {
  const remoteAllowed = status.mode !== "local";
  const stoppedBySafety = status.emergency || status.error !== "none" ||
    status.motion_inhibited;
  const canMove = state.authenticated && remoteAllowed && status.referenced &&
    !stoppedBySafety;
  $("#move-up").disabled = !canMove;
  $("#move-down").disabled = !canMove;
  $("#calibrate").disabled = !state.authenticated || !remoteAllowed ||
    status.emergency || status.error !== "none" || status.motion_inhibited ||
    status.state === "moving_up" || status.state === "moving_down";
  $$(".preset-motion").forEach((button) => {
    const preset = status.presets[Number(button.dataset.slot)];
    button.disabled = !canMove || !preset?.valid;
  });
  const canCapture = state.authenticated && status.state === "idle" &&
    status.error === "none" && status.referenced && !status.feedback_warning &&
    !status.emergency && !status.remote_active && !status.motion_inhibited;
  $$(".preset-save").forEach((button) => {
    button.disabled = !canCapture;
  });
  $("#update-firmware").disabled = !state.authenticated || status.remote_active ||
    status.motion_inhibited || !["idle", "unreferenced", "error"].includes(status.state) ||
    $("#firmware-file").files.length !== 1;
  $("#clear-error").disabled = !state.authenticated || status.error === "none" ||
    status.emergency || status.remote_active || status.motion_inhibited;
  $("#clear-error").classList.toggle("hidden", status.error === "none");
  $$(".segments button").forEach((button) => {
    button.classList.toggle("active", button.dataset.mode === status.mode);
    button.disabled = !state.authenticated || status.remote_active ||
      status.motion_inhibited;
  });
  const ready = canMove;
  $("#motion-lock").textContent = ready ? "Ready" : "Locked";
  $("#motion-lock").classList.toggle("ready", ready);
  $("#control-reason").textContent = movementReason(status);
}

function renderStatus(status) {
  state.status = status;
  state.authenticated = true;
  state.authPromptDismissed = false;
  setIndicator("api", status.wifi_connected ? "WiFi connected" : "Direct connection", "online");
  if (state.socket === null) {
    setIndicator("socket", "Control idle", "idle");
  }
  $("#last-update").textContent = "Updated just now";
  $("#session-command").textContent = "Sign out";
  $("#desk-state").textContent = humanize(status.state);
  $("#control-source").textContent = humanize(status.source);
  $("#mode-label").textContent = humanize(status.mode);
  $("#firmware-version").textContent = `${status.firmware} on ${status.partition}`;
  $("#free-heap").textContent = `${status.diagnostics.free_heap} B`;
  $("#minimum-heap").textContent = `${status.diagnostics.minimum_free_heap} B`;
  $("#dropped-logs").textContent = String(status.diagnostics.dropped_logs);
  $("#reset-reason").textContent = String(status.reset_reason);

  const average = status.axes.reduce((sum, axis) => sum + axis.position_um, 0) /
    status.axes.length / 1000;
  $("#desk-height").textContent = status.referenced ? average.toFixed(1) : "--";
  const positions = status.axes.map((axis) => axis.position_um);
  const skew = (Math.max(...positions) - Math.min(...positions)) / 1000;
  $("#skew-value").textContent = `Skew ${skew.toFixed(1)} mm`;
  status.axes.forEach(updateAxis);
  status.presets.forEach((preset, index) => {
    $(`#preset-${index + 1}-height`).textContent =
      preset.valid ? `${(preset.target_um / 1000).toFixed(1)} mm` : "Not set";
  });

  const alert = $("#alert");
  const hasAlert = status.error !== "none" || status.emergency ||
    status.feedback_warning || status.preset_storage_fault;
  alert.classList.toggle("hidden", !hasAlert);
  if (hasAlert) {
    $("#alert-title").textContent = status.emergency ? "Emergency stop active" :
      status.feedback_warning ? "Column skew warning" :
        status.preset_storage_fault ? "Preset storage unavailable" : "Motion stopped";
    $("#alert-detail").textContent = status.error === "none" ?
      "Inspect the reported condition before moving the desk." : humanize(status.error);
  }
  $("#provisioning").classList.toggle("hidden", !status.provisioning);
  updateControls(status);
  if (loginDialog.open) {
    loginDialog.close();
  }
}

function handleStatusFailure(error) {
  stopMotion("Control unavailable");
  setIndicator("api", error.status === 401 ? "Sign in required" : "API unavailable",
    error.status === 401 ? "pending" : "offline");
  $("#last-update").textContent = "Status unavailable";
  if (error.status === 401) {
    state.authenticated = false;
    $("#session-command").textContent = "Sign in";
    movementButtons.forEach((button) => {
      button.disabled = true;
    });
    $("#motion-lock").textContent = "Locked";
    $("#motion-lock").classList.remove("ready");
    $("#control-reason").textContent = "Sign in to enable remote control.";
    openLogin();
  }
}

async function pollStatus() {
  if (state.polling) {
    return;
  }
  state.polling = true;
  if (state.pollTimer !== null) {
    clearTimeout(state.pollTimer);
    state.pollTimer = null;
  }
  try {
    renderStatus(await request("/api/v1/status"));
  } catch (error) {
    handleStatusFailure(error);
  } finally {
    state.polling = false;
    state.pollTimer = setTimeout(pollStatus, STATUS_POLL_MS);
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

function stopMotion(socketLabel = "Control idle") {
  if (state.socketConnectTimer !== null) {
    clearTimeout(state.socketConnectTimer);
    state.socketConnectTimer = null;
  }
  if (state.heartbeat !== null) {
    clearInterval(state.heartbeat);
    state.heartbeat = null;
  }
  movementButtons.forEach((button) => button.classList.remove("pressed"));
  const socket = state.socket;
  if (socket?.readyState === WebSocket.OPEN) {
    sendCommand(0);
  }
  state.socket = null;
  state.pointerId = null;
  state.action = 0;
  if (socket !== null && socket.readyState < WebSocket.CLOSING) {
    socket.close(1000, "released");
  }
  setIndicator("socket", socketLabel, "idle");
}

function beginMotion(button, event) {
  if (button.disabled || state.socket !== null || state.pointerId !== null) {
    return;
  }
  if (location.protocol !== "https:") {
    setIndicator("socket", "HTTPS required", "failed");
    showToast("Remote motion requires HTTPS.", true);
    return;
  }
  event.preventDefault();
  button.setPointerCapture(event.pointerId);
  state.pointerId = event.pointerId;
  state.action = Number(button.dataset.action);
  state.sequence = 0;
  button.classList.add("pressed");
  setIndicator("socket", "Control connecting", "connecting");

  const socket = new WebSocket(
    `wss://${location.host}/api/v1/control`,
    "desk-control-v1",
  );
  socket.binaryType = "arraybuffer";
  state.socket = socket;
  state.socketConnectTimer = setTimeout(() => {
    if (state.socket === socket && socket.readyState !== WebSocket.OPEN) {
      setIndicator("socket", "Control timed out", "failed");
      showToast("The control channel did not connect.", true);
      stopMotion("Control unavailable");
    }
  }, SOCKET_CONNECT_TIMEOUT_MS);

  socket.addEventListener("open", () => {
    if (state.socket !== socket || state.pointerId === null) {
      socket.close(1000, "released");
      return;
    }
    clearTimeout(state.socketConnectTimer);
    state.socketConnectTimer = null;
    setIndicator("socket", "Control live", "live");
    sendCommand(state.action);
    state.heartbeat = setInterval(() => sendCommand(state.action), HEARTBEAT_MS);
  });
  socket.addEventListener("close", () => {
    if (state.socket === socket) {
      const wasHeld = state.pointerId !== null;
      stopMotion(wasHeld ? "Control interrupted" : "Control idle");
      if (wasHeld) {
        setIndicator("socket", "Control interrupted", "failed");
        showToast("Movement stopped because the control channel closed.", true);
      }
    }
  });
  socket.addEventListener("error", () => {
    if (state.socket === socket) {
      setIndicator("socket", "Control failed", "failed");
      showToast("Unable to open the control channel.", true);
      stopMotion("Control unavailable");
    }
  });
}

movementButtons.forEach((button) => {
  button.addEventListener("pointerdown", (event) => beginMotion(button, event));
  button.addEventListener("pointerup", () => stopMotion());
  button.addEventListener("pointercancel", () => stopMotion());
  button.addEventListener("lostpointercapture", () => stopMotion());
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
    state.authPromptDismissed = false;
    await pollStatus();
    showToast("Signed in.");
  } catch (error) {
    $("#login-error").textContent = humanize(error.message);
  }
});

$("#session-command").addEventListener("click", async () => {
  if (!state.authenticated) {
    state.authPromptDismissed = false;
    openLogin(true);
    return;
  }
  stopMotion();
  try {
    await request("/api/v1/session", {method: "DELETE"});
  } catch (_) {
    // Local state is cleared even if the device closed the connection first.
  }
  state.authenticated = false;
  state.authPromptDismissed = false;
  $("#session-command").textContent = "Sign in";
  openLogin(true);
});

$$(".segments button").forEach((button) => {
  button.addEventListener("click", async () => {
    stopMotion();
    const accepted = await askConfirmation(
      "Change control mode?",
      `Switch to ${button.textContent.trim()}. Active remote movement is stopped first.`,
      "Change mode",
    );
    if (!accepted) {
      return;
    }
    try {
      await request("/api/v1/mode", {
        method: "POST",
        headers: {"Content-Type": "application/x-www-form-urlencoded"},
        body: new URLSearchParams({mode: button.dataset.mode}),
      });
      showToast("Control mode updated.");
    } catch (error) {
      showToast(humanize(error.message), true);
    }
    await pollStatus();
  });
});

$$(".preset-save").forEach((button) => {
  button.addEventListener("click", async () => {
    stopMotion();
    const slot = Number(button.dataset.slot) + 1;
    if (!await askConfirmation(
      `Save preset M${slot}?`,
      "The current calibrated height will replace the existing value.",
      "Save preset",
    )) {
      return;
    }
    try {
      await request("/api/v1/preset", {
        method: "POST",
        headers: {"Content-Type": "application/x-www-form-urlencoded"},
        body: new URLSearchParams({slot: String(slot)}),
      });
      showToast(`Preset M${slot} saved.`);
    } catch (error) {
      showToast(humanize(error.message), true);
    }
    await pollStatus();
  });
});

$("#firmware-file").addEventListener("change", () => {
  if (state.status !== null) {
    updateControls(state.status);
  }
});

$("#clear-error").addEventListener("click", async () => {
  stopMotion();
  if (!await askConfirmation(
    "Clear the latched error?",
    "The controller will re-check every safety input and return to Safe Start. The desk will not move.",
    "Clear error",
  )) {
    return;
  }
  try {
    await request("/api/v1/error/clear", {method: "POST"});
    showToast("Clear request accepted. Safety inputs are being checked.");
  } catch (error) {
    showToast(humanize(error.message), true);
  }
  await pollStatus();
});

$("#update-firmware").addEventListener("click", async () => {
  stopMotion();
  const file = $("#firmware-file").files[0];
  if (file === undefined || !await askConfirmation(
    "Install firmware update?",
    "Motion will remain inhibited while the signed image is checked and installed.",
    "Install and restart",
  )) {
    return;
  }
  $("#update-firmware").disabled = true;
  $("#update-progress").classList.remove("hidden");
  try {
    await request("/api/v1/update", {
      method: "POST",
      headers: {"Content-Type": "application/octet-stream"},
      body: file,
    });
    setIndicator("api", "Device restarting", "pending");
    showToast("Update accepted. The controller is restarting.");
  } catch (error) {
    $("#update-progress").classList.add("hidden");
    showToast(humanize(error.message), true);
    await pollStatus();
  }
});

$("#wifi-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  stopMotion();
  const form = new FormData(event.currentTarget);
  if (!await askConfirmation(
    "Connect to this WiFi network?",
    "The provisioning access point may close while the controller joins the network.",
    "Connect",
  )) {
    return;
  }
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
    showToast("WiFi credentials accepted. Reconnecting...");
  } catch (error) {
    showToast(humanize(error.message), true);
    await pollStatus();
  }
});

$("#toggle-wifi-password").addEventListener("click", () => {
  const input = $("#wifi-password");
  const show = input.type === "password";
  input.type = show ? "text" : "password";
  $("#toggle-wifi-password").textContent = show ? "Hide" : "Show";
  $("#toggle-wifi-password").setAttribute("aria-label", show ? "Hide password" : "Show password");
});

$("#clear-wifi-form").addEventListener("click", () => $("#wifi-form").reset());

$$("[data-close-dialog]").forEach((button) => {
  button.addEventListener("click", closeLogin);
});

$$("[data-confirm='cancel']").forEach((button) => {
  button.addEventListener("click", () => resolveConfirmation(false));
});

$("#confirm-form").addEventListener("submit", (event) => {
  event.preventDefault();
  resolveConfirmation(true);
});

loginDialog.addEventListener("cancel", (event) => {
  event.preventDefault();
  closeLogin();
});

confirmDialog.addEventListener("cancel", (event) => {
  event.preventDefault();
  resolveConfirmation(false);
});

window.addEventListener("blur", () => stopMotion());
window.addEventListener("pagehide", () => stopMotion());
document.addEventListener("visibilitychange", () => {
  if (document.hidden) {
    stopMotion();
  }
});

if (location.protocol !== "https:") {
  setIndicator("api", "HTTPS required", "failed");
  $("#control-reason").textContent = "Open this controller over HTTPS.";
} else {
  pollStatus();
}
