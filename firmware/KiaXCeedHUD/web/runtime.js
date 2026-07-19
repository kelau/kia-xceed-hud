(function () {
  const state = (window.HUD_RUNTIME = {
    connected: false,
    reconnects: 0,
    messages: 0,
    lastMessageAt: 0,
    lastLatencyMs: 0,
    peakLatencyMs: 0,
    compatibility: null,
    errors: window.HUD_BOOT_ERRORS || [],
  });
  window.HUD_STREAMING = typeof EventSource !== "undefined";

  window.addEventListener("error", (event) => {
    state.errors.push(event.message || "Unknown browser error");
    renderDiagnostics();
  });
  window.addEventListener("unhandledrejection", (event) => {
    state.errors.push(String(event.reason || "Unhandled promise rejection"));
    renderDiagnostics();
  });

  async function compatibility() {
    const started = performance.now();
    const response = await fetch("/api/compatibility", { cache: "no-store" });
    state.lastLatencyMs = performance.now() - started;
    state.peakLatencyMs = Math.max(state.peakLatencyMs, state.lastLatencyMs);
    if (!response.ok) throw new Error(`Compatibility API returned ${response.status}`);
    state.compatibility = await response.json();
    if (state.compatibility.uiVersion !== "0.16.1" || state.compatibility.apiVersion !== 2)
      throw new Error(`Incompatible Web UI: expected UI 0.16.1 / API 2, received UI ${state.compatibility.uiVersion} / API ${state.compatibility.apiVersion}`);
    window.HUD_STARTED = true;
    const panel = document.getElementById("bootError");
    if (panel) panel.hidden = true;
    renderDiagnostics();
  }

  function callIfReady(name) {
    try {
      const page = typeof activePage === "undefined" ? "" : activePage;
      if (name === "metrics" && (page === "status" || page === "display") && typeof refresh === "function") refresh();
      if (name === "performance" && page === "status" && typeof refreshPerformance === "function") refreshPerformance();
      if (name === "can" && page === "status" && typeof refreshCanStatus === "function") refreshCanStatus();
      if (name === "frames" && page === "frames" && typeof loadFrames === "function") {
        loadFrames();
        window.HUD_REFRESH_SELECTED_FRAME?.();
      }
    } catch (error) {
      state.errors.push(String(error));
    }
  }

  function connect() {
    if (!window.HUD_STREAMING) return;
    const stream = new EventSource("/api/events");
    stream.onopen = () => {
      if (state.lastMessageAt) state.reconnects++;
      state.connected = true;
      renderDiagnostics();
    };
    stream.onerror = () => {
      state.connected = false;
      renderDiagnostics();
    };
    ["metrics", "performance", "can", "frames", "config-saved"].forEach((name) =>
      stream.addEventListener(name, (event) => {
        state.messages++;
        state.lastMessageAt = Date.now();
        if (name === "config-saved") window.dispatchEvent(new CustomEvent("hud-config-saved", { detail: event.data }));
        else callIfReady(name);
        renderDiagnostics();
      }),
    );
  }

  function renderDiagnostics() {
    if (typeof activePage !== "undefined" && activePage !== "diagnostics") return;
    const target = document.getElementById("uiDiagnostics");
    if (!target) return;
    const age = state.lastMessageAt ? Date.now() - state.lastMessageAt : null;
    const compatibilityText = state.compatibility
      ? `API ${state.compatibility.apiVersion}, schema ${state.compatibility.configSchemaVersion}, firmware ${state.compatibility.firmwareVersion}`
      : "Compatibility check pending";
    target.innerHTML = `<p><b>${state.connected ? "Connected" : "Disconnected"}</b> · ${compatibilityText}</p>
      <p>Messages ${state.messages} · reconnects ${state.reconnects} · stream age ${age === null ? "—" : age + " ms"}</p>
      <p>API latency ${state.lastLatencyMs.toFixed(1)} ms · peak ${state.peakLatencyMs.toFixed(1)} ms</p>`;
    const errors = document.getElementById("uiErrors");
    if (errors) errors.textContent = state.errors.length ? state.errors.join("\n") : "None";
  }

  compatibility().catch((error) => {
    state.errors.push(String(error));
    const panel = document.getElementById("bootError");
    if (panel) {
      panel.hidden = false;
      panel.querySelector("pre").textContent = String(error);
    }
    renderDiagnostics();
  });
  window.addEventListener("DOMContentLoaded", () => {
    if (typeof pages !== "undefined" && !pages.includes("diagnostics")) pages.splice(pages.length - 1, 0, "diagnostics");
    const nav = document.getElementById("nav");
    if (nav && !nav.querySelector('[data-page="diagnostics"]')) {
      const button = document.createElement("button");
      button.dataset.page = "diagnostics";
      button.textContent = "Diagnostics";
      button.onclick = () => show("diagnostics");
      nav.insertBefore(button, nav.lastElementChild);
    }
    renderDiagnostics();
  });
  connect();
  window.HUD_RENDER_DIAGNOSTICS = renderDiagnostics;
  setInterval(renderDiagnostics, 1000);
})();
