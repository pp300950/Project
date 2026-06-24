/* ═══════════════════════════════════════════════════════════
   Smart Shoe Monitor — shared.js
   ใช้ร่วมกันทุกหน้า: Auth, API fetch, UI helpers
   ═══════════════════════════════════════════════════════════ */

'use strict';

/* ── API Base URL ──────────────────────────────────────────
   Frontend serve จาก Render origin เดียวกับ Backend เสมอ
   fallback localhost สำหรับ dev บนเครื่อง
   ─────────────────────────────────────────────────────────── */
const API_BASE = (() => {
  if (location.protocol === 'file:') return 'http://localhost:8000';
  return location.origin;
})();


/* ── Token helpers ─────────────────────────────────────── */

function getToken() {
  return localStorage.getItem('ssm_token');
}

function getUser() {
  try {
    return JSON.parse(localStorage.getItem('ssm_user') || 'null');
  } catch {
    return null;
  }
}

function clearSession() {
  localStorage.removeItem('ssm_token');
  localStorage.removeItem('ssm_user');
}

/**
 * ตรวจว่า JWT ยังไม่หมดอายุ (parse payload ฝั่ง client)
 * ไม่ได้ verify signature — ใช้แค่เพื่อ UX ไม่ใช่ security
 */
function tokenIsValid(token) {
  if (!token) return false;
  try {
    const payload = JSON.parse(atob(token.split('.')[1]));
    return payload.exp ? Date.now() / 1000 < payload.exp : true;
  } catch {
    return false;
  }
}


/* ── requireAuth ───────────────────────────────────────────
   เรียกบน top ของทุกหน้าที่ต้อง login
   ถ้าไม่มี token หรือหมดอายุ → redirect ไป login.html
   ─────────────────────────────────────────────────────────── */
function requireAuth() {
  const token = getToken();
  if (!tokenIsValid(token)) {
    clearSession();
    sessionStorage.setItem('ssm_login_msg', 'เซสชันหมดอายุ กรุณาเข้าสู่ระบบใหม่');
    sessionStorage.setItem('ssm_redirect_after_login', location.href);
    location.replace('login.html');
  }
}


/* ── authFetch ─────────────────────────────────────────────
   Wrapper รอบ fetch ที่แนบ Authorization header อัตโนมัติ
   Throws object { status, message } เมื่อ response ไม่ ok
   เมื่อได้ 401 → redirect ไป login.html
   ─────────────────────────────────────────────────────────── */
async function authFetch(path, options = {}) {
  const token = getToken();
  const url = path.startsWith('http') ? path : `${API_BASE}${path}`;

  const res = await fetch(url, {
    ...options,
    headers: {
      'Content-Type': 'application/json',
      ...(token ? { Authorization: `Bearer ${token}` } : {}),
      ...(options.headers || {}),
    },
  });

  if (res.status === 401) {
    clearSession();
    sessionStorage.setItem('ssm_login_msg', 'เซสชันหมดอายุ กรุณาเข้าสู่ระบบใหม่');
    sessionStorage.setItem('ssm_redirect_after_login', location.href);
    location.replace('login.html');
    // throw เพื่อหยุด caller code
    const err = new Error('Unauthorized');
    err.status = 401;
    throw err;
  }

  if (!res.ok) {
    let message = `Server error ${res.status}`;
    try {
      const data = await res.json();
      if (data.detail) message = data.detail;
    } catch { /* ignore */ }
    const err = new Error(message);
    err.status = res.status;
    throw err;
  }

  return res.json();
}


/* ── initHeaderChrome ──────────────────────────────────────
   เติมชื่อผู้ดูแลในหัว + ผูก logout button
   เรียกหลัง requireAuth() ทุกหน้า
   ─────────────────────────────────────────────────────────── */
function initHeaderChrome() {
  // แสดงชื่อผู้ดูแล
  const user = getUser();
  const headerUserEl = document.getElementById('headerUser');
  if (headerUserEl && user && user.username) {
    headerUserEl.textContent = user.username;
  }

  // Logout button
  const logoutBtn = document.getElementById('logoutBtn');
  if (logoutBtn) {
    logoutBtn.addEventListener('click', () => {
      clearSession();
      location.replace('login.html');
    });
  }
}


/* ── Label helpers ─────────────────────────────────────────
   ใช้ใน dashboard.html, log.html, report.html
   ─────────────────────────────────────────────────────────── */

/**
 * แปลง event_type → ป้ายภาษาไทย
 */
function eventTypeLabel(type) {
  switch (type) {
    case 'zone_breach':  return 'เข้าใกล้จุดอันตราย';
    case 'shoe_removed': return 'ถอดรองเท้า';
    case 'step_data':    return 'ข้อมูลการเดิน';
    case 'heartbeat':    return 'heartbeat';
    default:             return type || '—';
  }
}

/**
 * แปลง zone key → ชื่อจุดเสี่ยงภาษาไทย
 */
function zoneLabel(zone) {
  switch (zone) {
    case 'fence': return 'รั้วบ้าน';
    case 'pool':  return 'สระว่ายน้ำ';
    default:      return zone || 'ไม่ระบุ';
  }
}

/**
 * แปลง distance_cm → string แสดงผล
 */
function fmtDistance(cm) {
  if (cm == null) return '—';
  return `${parseFloat(cm).toFixed(1)} cm`;
}

/**
 * คืน severity class (danger / warning / ok / info)
 * ใช้กับ chip และ event-dot
 */
function eventSeverity(event) {
  if (event.event_type === 'zone_breach') {
    const d = event.distance_cm ?? 999;
    if (d < 10) return 'danger';
    return 'warning';
  }
  if (event.event_type === 'shoe_removed') return 'warning';
  if (event.event_type === 'step_data')    return 'ok';
  if (event.event_type === 'heartbeat')    return 'info';
  return 'neutral';
}

/**
 * Format timestamp (ISO string หรือ Date) → วันที่/เวลาภาษาไทย
 * @param {string|Date} ts
 * @param {'full'|'date'|'time'} mode
 */
function fmtDate(ts, mode = 'full') {
  if (!ts) return '—';
  const d = (ts instanceof Date) ? ts : new Date(ts);
  if (isNaN(d)) return '—';
  const opts = {
    full: { day: '2-digit', month: '2-digit', year: 'numeric', hour: '2-digit', minute: '2-digit', hour12: false },
    date: { day: '2-digit', month: '2-digit', year: 'numeric' },
    time: { hour: '2-digit', minute: '2-digit', hour12: false },
  }[mode];
  return d.toLocaleString('th-TH', opts);
}
