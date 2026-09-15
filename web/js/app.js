import { sourceFromLocation } from "./source.js";
import { el, text, pending } from "./dom.js";
import { createModel, addEvents, addContacts, isEmpty, contactsForDisplay } from "./model.js";
import { renderDashboard } from "./views/dashboard.js";
import { renderContacts } from "./views/contacts.js";
import { renderProcesses } from "./views/processes.js";
import { renderLog } from "./views/log.js";
import { renderSituation } from "./views/situation.js";
import { renderBench } from "./views/bench.js";

const POLL_MS = 1000;

const LIVE_HISTORY = 120;

const TRAIL_POINTS = 12;

const VIEWS = [
    { id: "dashboard", label: "Dashboard",   icon: "◈", title: "System overview", sub: "The operations centre, from your own run" },
    { id: "situation", label: "Situation",   icon: "◎", title: "The user story",  sub: "The seven states of Section 5" },
    { id: "contacts",  label: "Contacts",    icon: "✈", title: "Contact reports", sub: "Every report the sensors sent" },
    { id: "processes", label: "Processes",   icon: "⚙", title: "Processes",       sub: "What the operating system is running" },
    { id: "log",       label: "Mission log", icon: "▤", title: "Mission log",     sub: "Every action, timestamped. R6" },
    { id: "bench",     label: "Bench",       icon: "▣", title: "The bench",       sub: "The map box: four sensors on one Pico 2W" },
];

const state = {
    source: sourceFromLocation(),
    model: createModel(),
    run: null,
    compare: null,
    tracks: null,

    trails: new Map(),
    live: null,
    liveSame: 0,

    livePrev: null,
    liveHistory: [],
    box: null,
    bench: null,
    benchPrev: null,
    benchSame: 0,
    view: "dashboard",
    offsets: { events: 0, contacts: 0 },
    waiting: { events: null, contacts: null },
    polls: 0,
};

function brand() {
    const NS = "http://www.w3.org/2000/svg";
    const svg = document.createElementNS(NS, "svg");
    svg.setAttribute("width", "30");
    svg.setAttribute("height", "34");
    svg.setAttribute("viewBox", "0 0 30 34");
    const shield = document.createElementNS(NS, "path");
    shield.setAttribute("d", "M15 1 L28 6 v11 c0 8-6 13-13 16C8 30 2 25 2 17V6z");
    shield.setAttribute("fill", "none");
    shield.setAttribute("stroke", "#7f96b0");
    shield.setAttribute("stroke-width", "1.6");
    const leaf = document.createElementNS(NS, "path");
    leaf.setAttribute("d",
        "M15 8 l1.6 3.4 3.1-1.2-1 3.3 3.3.4-2.4 2.2 2.9 2.2-3.6.6.5 3.3" +
        "-3.2-1.7-.6 3.9h-.4l-.6-3.9-3.2 1.7.5-3.3-3.6-.6 2.9-2.2-2.4-2.2 3.3-.4" +
        "-1-3.3 3.1 1.2z");
    leaf.setAttribute("fill", "#c8332c");
    svg.append(shield, leaf);
    return svg;
}

function buildRail(onPick) {
    const rail = el("aside", { class: "rail" });

    rail.append(el("div", { class: "rail-brand" },
        brand(),
        el("div", {},
            el("h1", {}, text("SENTINEL")),
            el("p", {}, text("Operations Centre")))));

    const nav = el("nav", {});
    VIEWS.forEach((v) => {
        nav.append(el("button", { "data-view": v.id, onclick: () => onPick(v.id) },
            el("span", { class: "ico" }, text(v.icon)),
            text(v.label),
            el("span", { class: "badge", "data-badge": v.id })));
    });
    rail.append(nav);

    rail.append(el("div", { class: "rail-foot" },
        el("span", { class: "dot", id: "live-dot" }),
        el("span", { id: "live-text" }, text("source: " + state.source.name))));

    return rail;
}

function updateBadges() {
    const m = state.model;
    const shown = contactsForDisplay(m).length;
    const n = {
        contacts: shown,
        log: m.events.length,
        processes: Object.keys(m.sensors).length + (m.self ? 1 : 0),
        bench: state.bench && state.benchSame < 3 && state.bench.board &&
               state.bench.board.connected ? "live" : "",
    };
    document.querySelectorAll(".badge").forEach((b) => {
        const v = n[b.dataset.badge];
        b.textContent = v ? String(v) : "";
    });

    const dot = document.getElementById("live-dot");
    const txt = document.getElementById("live-text");
    if (!dot || !txt) return;
    if (isEmpty(m)) {
        dot.className = "dot idle";
        txt.textContent = "waiting for data";
    } else {
        dot.className = "dot";
        txt.textContent = m.events.length + " log  ·  " + shown + " contacts";
    }
}

function setView(id) {
    state.view = id;
    const meta = VIEWS.find((v) => v.id === id) || VIEWS[0];

    document.querySelectorAll(".rail nav button").forEach((b) => {
        if (b.dataset.view === id) b.setAttribute("aria-current", "page");
        else b.removeAttribute("aria-current");
    });

    document.getElementById("view-title").textContent = meta.title;
    document.getElementById("view-sub").textContent = meta.sub;

    draw();
}

function draw() {
    const host = document.getElementById("content");
    if (!host) return;
    host.replaceChildren();

    switch (state.view) {
    case "dashboard":
        renderDashboard(host, state.model, state.run, state.tracks,
                        state.live, state.liveSame, state.livePrev,
                        state.trails, state.compare);
        break;
    case "contacts":
        renderContacts(host, state.model, state.tracks);
        break;
    case "processes":
        renderProcesses(host, state.model, state.live);
        break;
    case "log":
        renderLog(host, state.model, state.waiting.events);
        break;
    case "bench":
        renderBench(host, state.bench, state.box, state.benchPrev, state.benchSame);
        break;
    case "situation":
        renderSituation(host, state.model, state.live, state.livePrev);
        break;
    default:
        host.append(pending("No such view", null, null));
    }
    updateBadges();
}

function updateTrails(tracks) {
    if (!tracks || !Array.isArray(tracks.tracks)) return;

    const seen = new Set();
    tracks.tracks.forEach((t) => {
        if (!isFinite(t.lat) || !isFinite(t.lon)) return;
        seen.add(t.id);

        let pts = state.trails.get(t.id);
        if (!pts) {
            pts = [];
            state.trails.set(t.id, pts);
        }
        const last = pts[pts.length - 1];
        if (!last || Math.abs(last.lat - t.lat) > 1e-4 ||
                     Math.abs(last.lon - t.lon) > 1e-4) {
            pts.push({ lat: t.lat, lon: t.lon });
            if (pts.length > TRAIL_POINTS) pts.shift();
        }
    });

    state.trails.forEach((_, id) => {
        if (!seen.has(id)) state.trails.delete(id);
    });
}

async function poll() {
    let changed = false;

    try {
        const ev = await state.source.events(state.offsets.events);
        if (ev.ok) {
            state.waiting.events = null;
            if (ev.offset < state.offsets.events) {
                state.model = createModel();
                state.offsets.contacts = 0;
            }
            if (ev.lines.length) { addEvents(state.model, ev.lines); changed = true; }
            state.offsets.events = ev.offset;
        } else {
            state.waiting.events = ev.reason;
        }
    } catch (e) {
        state.waiting.events = String(e.message || e);
    }

    try {
        const co = await state.source.contacts(state.offsets.contacts);
        if (co.ok) {
            state.waiting.contacts = null;
            if (co.lines.length) { addContacts(state.model, co.lines); changed = true; }
            state.offsets.contacts = co.offset;
        } else {
            state.waiting.contacts = co.reason;
        }
    } catch (e) {
        state.waiting.contacts = String(e.message || e);
    }

    try {
        const r = await state.source.run();
        if (r.ok && r.data) {
            if (JSON.stringify(r.data) !== JSON.stringify(state.run)) {
                changed = true;
            }
            state.run = r.data;
        }
    } catch (e) { }

    try {
        const t = await state.source.tracks();
        if (t.ok && t.data) {
            if (!state.tracks || t.data.count !== state.tracks.count ||
                t.data.reports !== state.tracks.reports) {
                changed = true;
            }
            if (state.tracks && t.data.reports < state.tracks.reports) {
                state.trails.clear();
            }
            state.tracks = t.data;
            updateTrails(t.data);
        }
    } catch (e) { }

    try {
        const c = await state.source.compare();
        if (c.ok && c.data) {
            if (JSON.stringify(c.data) !== JSON.stringify(state.compare)) {
                state.compare = c.data;
                changed = true;
            }
        }
    } catch (e) { }

    try {
        const lv = await state.source.live();
        if (lv.ok && lv.data) {
            if (!state.live || lv.data.written !== state.live.written) {
                state.livePrev = state.live;
                state.live = lv.data;
                state.liveSame = 0;
                changed = true;

                state.liveHistory.push({
                    written: lv.data.written,
                    cpus: lv.data.cpus,
                    pool_cpu_ns: lv.data.pool_cpu_ns,
                    queue: lv.data.queue,
                });
                if (state.liveHistory.length > LIVE_HISTORY) {
                    state.liveHistory.shift();
                }
            } else {
                state.liveSame++;
            }
        }
    } catch (e) { }

    try {
        const b = await state.source.bench();
        if (b.ok && b.data) {
            if (!state.bench || b.data.written !== state.bench.written) {
                state.benchPrev = state.bench;
                state.bench = b.data;
                state.benchSame = 0;
            } else {
                state.benchSame++;
            }
            if (state.view === "bench") changed = true;
        } else if (state.bench) {
            state.benchSame++;
            if (state.view === "bench") changed = true;
        }
    } catch (e) { }

    state.polls++;

    if (changed) draw();
    else updateBadges();
}

async function boot() {
    const app = el("div", { class: "app" });

    const main = el("main", { class: "main" },
        el("header", { class: "topbar" },
            el("div", {},
                el("h2", { id: "view-title" }, text("")),
                el("div", { class: "sub", id: "view-sub" }, text(""))),
            el("span", { class: "spacer" }),
            el("div", { class: "sub", id: "counts" }, text(""))),
        el("div", { class: "content", id: "content" }));

    app.append(buildRail(setView), main);
    document.body.replaceChildren(app);

    const bx = await state.source.box();
    state.box = bx && bx.ok ? bx.data : null;

    await poll();

    const asked = new URLSearchParams(window.location.search).get("view");
    if (asked && VIEWS.some((v) => v.id === asked)) {
        setView(asked);
    } else {
        setView("dashboard");
    }

    setInterval(poll, POLL_MS);
}

boot();
