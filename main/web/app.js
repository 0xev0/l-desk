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

const STATE_LABELS = {
  safe_start: "Starting",
  unreferenced: "Needs calibration",
  idle: "Ready",
  moving_up: "Moving up",
  moving_down: "Moving down",
  calibrating_down: "Calibrating",
  error: "Stopped",
};

const ERROR_LABELS = {
  none: "No fault",
  emergency_stop: "E-stop engaged",
  both_buttons: "Up and down were pressed together",
  alignment_timeout: "Columns did not reach the endstop together",
  calibration_timeout: "Calibration took too long",
  limit_conflict: "Conflicting endstop signals",
  invalid_transition: "Unexpected controller transition",
  hardware_fault: "Hardware input or output fault",
  control_loop_overrun: "Control loop missed its deadline",
  feedback_missing: "A Hall signal is missing",
  feedback_direction: "A column moved in the wrong direction",
  feedback_stall: "A column stopped reporting movement",
  feedback_skew: "Columns moved too far out of level",
};

const MODE_LABELS = {
  local: "Buttons",
  hybrid: "Buttons + phone",
  mobile: "Phone",
};

const SOURCE_LABELS = {
  none: "No active input",
  local: "Buttons",
  remote: "Phone",
};

const MESSAGE_LABELS = {
  authentication_required: "Enter the device code to continue.",
  invalid_credentials: "That device code was not accepted.",
  rate_limited: "Too many attempts. Wait a moment and try again.",
  request_timeout: "The desk did not respond in time.",
  control_unavailable: "The desk controller is not responding.",
  clear_error_forbidden: "The fault cannot be reset in the current state.",
  clear_error_queue_full: "The reset request could not be queued.",
  error_not_clearable: "Resolve the reported condition before resetting the fault.",
  desk_must_be_idle: "Stop the desk before doing that.",
  desk_must_be_stopped: "Stop the desk before doing that.",
  invalid_network: "Check the network name and password.",
  network_apply_failed: "The desk could not apply the Wi-Fi settings.",
  preset_capture_unsafe: "The current height cannot be saved.",
  update_rejected: "The firmware file was rejected.",
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

function labelFor(labels, value) {
  return labels[value] || humanize(value);
}

function errorLabel(error) {
  return labelFor(ERROR_LABELS, error);
}

function messageLabel(message) {
  return MESSAGE_LABELS[message] || humanize(message);
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
  const position = Math.max(0, Math.min(100, axis.position_um / 5000));
  $(`#axis-${id}-position`).textContent = `${millimeters.toFixed(1)} mm`;
  $(`#axis-${id}-level`).parentElement.style.setProperty("--position", `${position}%`);
  $(`#axis-${id}-lower`).classList.toggle("active", axis.lower);
  $(`#axis-${id}-upper`).classList.toggle("active", axis.upper);
  $(`#axis-${id}-hall`).textContent = axis.hall_valid ? `Hall ${axis.hall}` : "No Hall signal";
  $(`#axis-${id}-hall`).classList.toggle("active", !axis.hall_valid);
}

function movementReason(status) {
  if (!state.authenticated) {
    return "Connect to use phone controls.";
  }
  if (status.mode === "local") {
    return "Phone control is off. Change the input mode below.";
  }
  if (status.emergency) {
    return "E-stop is engaged.";
  }
  if (status.error !== "none") {
    return errorLabel(status.error);
  }
  if (!status.referenced) {
    return "Calibrate before moving up or using presets.";
  }
  if (status.motion_inhibited) {
    return "Firmware maintenance is in progress.";
  }
  return "Ready";
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
  $("#motion-lock").textContent = ready ? "Ready" : "Blocked";
  $("#motion-lock").classList.toggle("ready", ready);
  $("#control-reason").textContent = movementReason(status);
}

function renderStatus(status) {
  state.status = status;
  state.authenticated = true;
  state.authPromptDismissed = false;
  setIndicator("api", status.wifi_connected ? "On Wi-Fi" : "Setup network", "online");
  if (state.socket === null) {
    setIndicator("socket", "Standby", "idle");
  }
  $("#last-update").textContent = "Updated now";
  $("#session-command").textContent = "Disconnect";
  $("#desk-state").textContent = labelFor(STATE_LABELS, status.state);
  $("#control-source").textContent = labelFor(SOURCE_LABELS, status.source);
  $("#mode-label").textContent = labelFor(MODE_LABELS, status.mode);
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
  $("#skew-value").textContent = `Level ${skew.toFixed(1)} mm`;
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
    $("#alert-title").textContent = status.emergency ? "E-stop engaged" :
      status.feedback_warning ? "Columns out of level" :
        status.preset_storage_fault ? "Presets unavailable" : "Desk stopped";
    $("#alert-detail").textContent = status.error !== "none" ?
      errorLabel(status.error) :
      status.feedback_warning ? `Column difference is ${skew.toFixed(1)} mm.` :
        status.preset_storage_fault ? "Saved heights could not be read." :
          "Resolve the reported condition before moving.";
  }
  $("#provisioning").classList.toggle("hidden", !status.provisioning);
  updateControls(status);
  if (loginDialog.open) {
    loginDialog.close();
  }
}

function handleStatusFailure(error) {
  stopMotion("Unavailable");
  setIndicator("api", error.status === 401 ? "Code required" : "Not responding",
    error.status === 401 ? "pending" : "offline");
  $("#last-update").textContent = "No recent status";
  if (error.status === 401) {
    state.authenticated = false;
    $("#session-command").textContent = "Connect";
    movementButtons.forEach((button) => {
      button.disabled = true;
    });
    $("#motion-lock").textContent = "Blocked";
    $("#motion-lock").classList.remove("ready");
    $("#control-reason").textContent = "Connect to use phone controls.";
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

function stopMotion(socketLabel = "Standby") {
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
    setIndicator("socket", "HTTPS needed", "failed");
    showToast("Open the desk over HTTPS to use phone controls.", true);
    return;
  }
  event.preventDefault();
  button.setPointerCapture(event.pointerId);
  state.pointerId = event.pointerId;
  state.action = Number(button.dataset.action);
  state.sequence = 0;
  button.classList.add("pressed");
  setIndicator("socket", "Connecting", "connecting");

  const socket = new WebSocket(
    `wss://${location.host}/api/v1/control`,
    "desk-control-v1",
  );
  socket.binaryType = "arraybuffer";
  state.socket = socket;
  state.socketConnectTimer = setTimeout(() => {
    if (state.socket === socket && socket.readyState !== WebSocket.OPEN) {
      setIndicator("socket", "Timed out", "failed");
      showToast("The motion link did not connect.", true);
      stopMotion("Unavailable");
    }
  }, SOCKET_CONNECT_TIMEOUT_MS);

  socket.addEventListener("open", () => {
    if (state.socket !== socket || state.pointerId === null) {
      socket.close(1000, "released");
      return;
    }
    clearTimeout(state.socketConnectTimer);
    state.socketConnectTimer = null;
    setIndicator("socket", "Live", "live");
    sendCommand(state.action);
    state.heartbeat = setInterval(() => sendCommand(state.action), HEARTBEAT_MS);
  });
  socket.addEventListener("close", () => {
    if (state.socket === socket) {
      const wasHeld = state.pointerId !== null;
      stopMotion(wasHeld ? "Connection lost" : "Standby");
      if (wasHeld) {
        setIndicator("socket", "Connection lost", "failed");
        showToast("Movement stopped because the motion link closed.", true);
      }
    }
  });
  socket.addEventListener("error", () => {
    if (state.socket === socket) {
      setIndicator("socket", "Unavailable", "failed");
      showToast("The motion link could not be opened.", true);
      stopMotion("Unavailable");
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
    showToast("Connected.");
  } catch (error) {
    $("#login-error").textContent = messageLabel(error.message);
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
  $("#session-command").textContent = "Connect";
  openLogin(true);
});

$$(".segments button").forEach((button) => {
  button.addEventListener("click", async () => {
    stopMotion();
    const accepted = await askConfirmation(
      "Change control inputs?",
      `Use ${button.textContent.trim()} for desk movement? Any phone movement stops first.`,
      "Change inputs",
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
      showToast("Control inputs updated.");
    } catch (error) {
      showToast(messageLabel(error.message), true);
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
      showToast(messageLabel(error.message), true);
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
    "Reset this fault?",
    "The desk will check its inputs and return to its startup state. It will stay still.",
    "Reset fault",
  )) {
    return;
  }
  try {
    await request("/api/v1/error/clear", {method: "POST"});
    showToast("Fault reset requested.");
  } catch (error) {
    showToast(messageLabel(error.message), true);
  }
  await pollStatus();
});

$("#update-firmware").addEventListener("click", async () => {
  stopMotion();
  const file = $("#firmware-file").files[0];
  if (file === undefined || !await askConfirmation(
    "Install firmware update?",
    "The desk will stay still while it checks the file, installs it, and restarts.",
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
    setIndicator("api", "Restarting", "pending");
    showToast("Update installed. The desk is restarting.");
  } catch (error) {
    $("#update-progress").classList.add("hidden");
    showToast(messageLabel(error.message), true);
    await pollStatus();
  }
});

$("#wifi-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  stopMotion();
  const form = new FormData(event.currentTarget);
  if (!await askConfirmation(
    "Join this Wi-Fi network?",
    "The desk's setup network will close after it connects.",
    "Join network",
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
    showToast("Wi-Fi saved. Reconnecting...");
  } catch (error) {
    showToast(messageLabel(error.message), true);
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
  setIndicator("api", "HTTPS needed", "failed");
  $("#control-reason").textContent = "Open this page over HTTPS to connect.";
} else {
  pollStatus();
}
