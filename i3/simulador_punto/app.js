const C = {
  centerX: 64,
  centerY: 32,
  width: 128,
  height: 64,
  radius: 4,
  margin: 5,
  splashMs: 1000,
  hpAlpha: 0.8,
  deadZone: 0.04,
  gainH: 23500,
  gainV: 14000,
  friction: 0.78,
  minVelocity: 0.1,
  directionBrake: 0.35,
  initialResponse: 0.32,
  targetVelocityH: 820,
  targetVelocityV: 500,
  signH: 1,
  signV: 1,
  rotThreshold: 35,
  gyroCooldownMs: 90,
  idleMs: 120,
  returnForce: 16,
  returnBrake: 0.95,
  intervalMs: 10,
};

const state = {
  px: C.centerX,
  py: C.centerY,
  vx: 0,
  vy: 0,
  hpH: 0,
  hpV: 0,
  prevRawH: 0,
  prevRawV: 0,
  lastMovement: performance.now(),
  lastRotation: 0,
  lastStep: performance.now(),
  factor: 1,
  moveH: 0,
  moveV: 0,
  rotDps: 0,
  splashStart: performance.now(),
  splashDone: false,
};

const sliders = {
  accelH: document.querySelector("#accelH"),
  accelV: document.querySelector("#accelV"),
  gyroX: document.querySelector("#gyroX"),
  gyroY: document.querySelector("#gyroY"),
  gyroZ: document.querySelector("#gyroZ"),
};

const outputs = {
  accelH: document.querySelector("#accelHValue"),
  accelV: document.querySelector("#accelVValue"),
  gyroX: document.querySelector("#gyroXValue"),
  gyroY: document.querySelector("#gyroYValue"),
  gyroZ: document.querySelector("#gyroZValue"),
  gate: document.querySelector("#gateLabel"),
  rot: document.querySelector("#rotValue"),
  px: document.querySelector("#pxValue"),
  py: document.querySelector("#pyValue"),
  meterH: document.querySelector("#meterH"),
  meterV: document.querySelector("#meterV"),
  meterGate: document.querySelector("#meterGate"),
};

const gateToggle = document.querySelector("#gateToggle");
const resetButton = document.querySelector("#resetButton");
const oledCanvas = document.querySelector("#oledCanvas");
const oledCtx = oledCanvas.getContext("2d");
const axisCanvas = document.querySelector("#axisCanvas");
const axisCtx = axisCanvas.getContext("2d");
const cupraLogoImage = new Image();
cupraLogoImage.src = "./assets/cupra_logo_white.png";

let pulse = { end: 0, h: 0, v: 0, gx: 0, gy: 0, gz: 0 };

function num(id) {
  return Number(sliders[id].value);
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function resetSimulation() {
  state.px = C.centerX;
  state.py = C.centerY;
  state.vx = 0;
  state.vy = 0;
  state.hpH = 0;
  state.hpV = 0;
  state.prevRawH = 0;
  state.prevRawV = 0;
  state.lastMovement = performance.now();
  state.lastRotation = 0;
}

function drawCupraLogo(ctx, cx, cy, scale, alpha) {
  if (!cupraLogoImage.complete || !cupraLogoImage.naturalWidth) return;

  const finalW = 72;
  const finalH = 55;
  const w = finalW * scale;
  const h = finalH * scale;

  ctx.save();
  ctx.globalAlpha = alpha;
  ctx.imageSmoothingEnabled = true;
  ctx.imageSmoothingQuality = "high";
  ctx.drawImage(cupraLogoImage, cx - w / 2, cy - h / 2, w, h);
  ctx.restore();
}

function drawSplash(now) {
  oledCanvas.classList.add("logo-mode");

  if (!cupraLogoImage.complete || !cupraLogoImage.naturalWidth) {
    state.splashStart = now;
    oledCtx.clearRect(0, 0, C.width, C.height);
    oledCtx.fillStyle = "#061015";
    oledCtx.fillRect(0, 0, C.width, C.height);
    return;
  }

  const elapsed = now - state.splashStart;
  const t = clamp(elapsed / C.splashMs, 0, 1);
  const grow = Math.min(t / 0.65, 1);
  const fadeOut = t < 0.72 ? 1 : clamp((1 - t) / 0.28, 0, 1);
  const scale = 0.3 + 0.7 * grow;

  oledCtx.clearRect(0, 0, C.width, C.height);
  oledCtx.fillStyle = "#061015";
  oledCtx.fillRect(0, 0, C.width, C.height);
  drawCupraLogo(oledCtx, C.centerX, C.centerY, scale, fadeOut);
}

function setPulse(kind) {
  const presets = {
    left:  { h: 0.28, v: 0, gx: 0, gy: 0, gz: 0 },
    right: { h: -0.28, v: 0, gx: 0, gy: 0, gz: 0 },
    up:    { h: 0, v: 0.26, gx: 0, gy: 0, gz: 0 },
    down:  { h: 0, v: -0.26, gx: 0, gy: 0, gz: 0 },
    rotX:  { h: 0.14, v: -0.12, gx: 120, gy: 0, gz: 0 },
    rotY:  { h: -0.16, v: 0.05, gx: 0, gy: 120, gz: 0 },
    rotZ:  { h: 0.11, v: 0.13, gx: 0, gy: 0, gz: 120 },
  };

  pulse = {
    end: performance.now() + 260,
    ...presets[kind],
  };
}

function currentInput(now) {
  const activePulse = now < pulse.end ? pulse : { h: 0, v: 0, gx: 0, gy: 0, gz: 0 };
  return {
    rawH: num("accelH") + activePulse.h,
    rawV: num("accelV") + activePulse.v,
    gyroX: num("gyroX") + activePulse.gx,
    gyroY: num("gyroY") + activePulse.gy,
    gyroZ: num("gyroZ") + activePulse.gz,
  };
}

function step(now) {
  const dt = Math.min((now - state.lastStep) / 1000, 0.04);
  state.lastStep = now;

  const input = currentInput(now);

  state.hpH = C.hpAlpha * (state.hpH + input.rawH - state.prevRawH);
  state.hpV = C.hpAlpha * (state.hpV + input.rawV - state.prevRawV);
  state.prevRawH = input.rawH;
  state.prevRawV = input.rawV;

  const rotDps = Math.sqrt(
    input.gyroX * input.gyroX +
    input.gyroY * input.gyroY +
    input.gyroZ * input.gyroZ
  );

  if (rotDps > C.rotThreshold) {
    state.lastRotation = now;
  }

  let factor = 1;
  if (gateToggle.checked && now - state.lastRotation < C.gyroCooldownMs) {
    factor = 0;
    const cleanup = 0.45;
    state.hpH *= cleanup;
    state.hpV *= cleanup;
  }

  let moveH = C.signH * state.hpH * factor;
  let moveV = C.signV * state.hpV * factor;

  if (Math.abs(moveH) < C.deadZone) moveH = 0;
  if (Math.abs(moveV) < C.deadZone) moveV = 0;

  const hasTranslation = moveH !== 0 || moveV !== 0;
  if (hasTranslation) state.lastMovement = now;

  if ((moveH > 0 && state.vx < 0) || (moveH < 0 && state.vx > 0)) {
    state.vx *= C.directionBrake;
  }
  if ((moveV > 0 && state.vy < 0) || (moveV < 0 && state.vy > 0)) {
    state.vy *= C.directionBrake;
  }

  if (moveH !== 0) {
    state.vx += (moveH * C.targetVelocityH - state.vx) * C.initialResponse;
  }
  if (moveV !== 0) {
    state.vy += (moveV * C.targetVelocityV - state.vy) * C.initialResponse;
  }

  state.vx += moveH * C.gainH * dt;
  state.vy += moveV * C.gainV * dt;

  if (!hasTranslation && now - state.lastMovement > C.idleMs) {
    state.vx += (C.centerX - state.px) * C.returnForce * dt;
    state.vy += (C.centerY - state.py) * C.returnForce * dt;
    state.vx *= C.returnBrake;
    state.vy *= C.returnBrake;
  }

  state.vx *= C.friction;
  state.vy *= C.friction;

  if (Math.abs(state.vx) < C.minVelocity) state.vx = 0;
  if (Math.abs(state.vy) < C.minVelocity) state.vy = 0;

  state.px += state.vx * dt;
  state.py += state.vy * dt;

  if (state.px < C.margin) { state.px = C.margin; state.vx = 0; }
  if (state.px > C.width - C.margin) { state.px = C.width - C.margin; state.vx = 0; }
  if (state.py < C.margin) { state.py = C.margin; state.vy = 0; }
  if (state.py > C.height - C.margin) { state.py = C.height - C.margin; state.vy = 0; }

  state.factor = factor;
  state.moveH = moveH;
  state.moveV = moveV;
  state.rotDps = rotDps;
}

function drawOled() {
  oledCtx.clearRect(0, 0, C.width, C.height);
  oledCtx.fillStyle = "#061015";
  oledCtx.fillRect(0, 0, C.width, C.height);

  oledCtx.strokeStyle = "rgba(72, 221, 236, 0.13)";
  oledCtx.lineWidth = 1;
  for (let x = 16; x < C.width; x += 16) {
    oledCtx.beginPath();
    oledCtx.moveTo(x, 0);
    oledCtx.lineTo(x, C.height);
    oledCtx.stroke();
  }
  for (let y = 16; y < C.height; y += 16) {
    oledCtx.beginPath();
    oledCtx.moveTo(0, y);
    oledCtx.lineTo(C.width, y);
    oledCtx.stroke();
  }

  oledCtx.strokeStyle = "rgba(255, 255, 255, 0.24)";
  oledCtx.strokeRect(C.margin, C.margin, C.width - C.margin * 2, C.height - C.margin * 2);

  oledCtx.fillStyle = "#f7ffff";
  oledCtx.beginPath();
  oledCtx.arc(state.px, state.py, C.radius, 0, Math.PI * 2);
  oledCtx.fill();
}

function drawAxis() {
  const w = axisCanvas.width;
  const h = axisCanvas.height;
  axisCtx.clearRect(0, 0, w, h);
  axisCtx.fillStyle = "#f7f9fc";
  axisCtx.fillRect(0, 0, w, h);

  const cx = 170;
  const cy = 96;
  axisCtx.lineWidth = 4;
  axisCtx.lineCap = "round";
  axisCtx.font = "700 15px Inter, system-ui, sans-serif";

  function axis(dx, dy, color, label) {
    axisCtx.strokeStyle = color;
    axisCtx.fillStyle = color;
    axisCtx.beginPath();
    axisCtx.moveTo(cx, cy);
    axisCtx.lineTo(cx + dx, cy + dy);
    axisCtx.stroke();
    axisCtx.beginPath();
    axisCtx.arc(cx + dx, cy + dy, 6, 0, Math.PI * 2);
    axisCtx.fill();
    axisCtx.fillText(label, cx + dx + 10, cy + dy + 5);
  }

  axis(94, 0, "#13b5c8", "Y");
  axis(0, -72, "#3cbf75", "Z");
  axis(-72, 48, "#d81b79", "X");

  axisCtx.fillStyle = "#15171c";
  axisCtx.font = "760 16px Inter, system-ui, sans-serif";
  axisCtx.fillText("Vista desde la caja", 18, 30);

  axisCtx.strokeStyle = "rgba(21, 23, 28, 0.18)";
  axisCtx.strokeRect(118, 58, 104, 54);
}

function renderReadouts() {
  outputs.accelH.value = `${num("accelH").toFixed(2)} g`;
  outputs.accelV.value = `${num("accelV").toFixed(2)} g`;
  outputs.gyroX.value = `${Math.round(num("gyroX"))} dps`;
  outputs.gyroY.value = `${Math.round(num("gyroY"))} dps`;
  outputs.gyroZ.value = `${Math.round(num("gyroZ"))} dps`;
  outputs.gate.textContent = gateToggle.checked ? "activo" : "libre";
  outputs.rot.textContent = Math.round(state.rotDps);
  outputs.px.textContent = state.px.toFixed(1);
  outputs.py.textContent = state.py.toFixed(1);

  outputs.meterH.style.width = `${clamp(Math.abs(state.moveH) / 0.35, 0, 1) * 100}%`;
  outputs.meterV.style.width = `${clamp(Math.abs(state.moveV) / 0.35, 0, 1) * 100}%`;
  outputs.meterGate.style.width = `${state.factor * 100}%`;
}

function animate(now) {
  if (!state.splashDone) {
    if (now - state.splashStart < C.splashMs) {
      drawSplash(now);
      renderReadouts();
      requestAnimationFrame(animate);
      return;
    }

    resetSimulation();
    state.lastStep = now;
    state.splashDone = true;
    oledCanvas.classList.remove("logo-mode");
  }

  step(now);
  drawOled();
  renderReadouts();
  requestAnimationFrame(animate);
}

Object.values(sliders).forEach((slider) => {
  slider.addEventListener("input", renderReadouts);
});

document.querySelectorAll("[data-pulse]").forEach((button) => {
  button.addEventListener("click", () => setPulse(button.dataset.pulse));
});

resetButton.addEventListener("click", resetSimulation);
gateToggle.addEventListener("change", renderReadouts);

drawAxis();
resetSimulation();
requestAnimationFrame(animate);
