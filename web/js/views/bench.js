import { el, text, chip, pending } from "../dom.js";

const NS = "http://www.w3.org/2000/svg";

const READING = {
    SONAR:    { unit: "range",  fmt: (v) => v.toFixed(1) + " cm" },
    OBSTACLE: { unit: "state",  fmt: (v) => (v >= 1 ? "near" : "clear") },
    BEAM:     { unit: "light",  fmt: (v) => String(Math.round(v)) },
    PIR:      { unit: "motion", fmt: (v) => (v >= 1 ? "motion" : "still") },
};

const SITE_NAME = {
    NWS: "North Warning radar", AUR: "CP-140 Aurora", AIS: "Coastal gate", SAT: "RADARSAT",
};

function svg(tag, attrs, ...children) {
    const n = document.createElementNS(NS, tag);
    for (const [k, v] of Object.entries(attrs || {})) n.setAttribute(k, String(v));
    for (const c of children) if (c) n.append(c);
    return n;
}

function svgText(x, y, str, cls, anchor) {
    const t = svg("text", { x, y, class: cls, "text-anchor": anchor || "start" });
    t.textContent = str;
    return t;
}

function stat(label, value, note) {
    return el("div", { class: "stat" },
        el("dt", {}, text(label)),
        el("dd", {}, text(value === null || value === undefined ? "—" : String(value))),
        note ? el("small", {}, text(note)) : null);
}

function sensorFor(bench, site) {
    return (bench.sensors || []).find((s) => s.site === site) || null;
}

function okChip(s) {
    if (!s || s.ok === null || s.ok === undefined) return chip("idle", "no report");
    return s.ok ? chip("ok", "answers") : chip("warn", "not ready");
}

function drawBox(box, bench, stale) {
    const S = 12, M = 46;
    const W = box.box.w_cm, H = box.box.h_cm;
    const X = (cm) => M + cm * S, Y = (cm) => M + cm * S;
    const g = svg("svg", {
        viewBox: "0 0 " + (W * S + 2 * M) + " " + (H * S + 2 * M),
        class: "bx" + (stale ? " bx-stale" : ""), role: "img",
        "aria-label": "Top view of the map box with the four sensors and their latest readings",
    });

    g.append(svg("rect", { x: X(0), y: Y(0), width: W * S, height: H * S, class: "bx-floor" }));

    const m = box.map;
    const lonX = (lon) => X(((lon - m.lon_west) / (m.lon_east - m.lon_west)) * W);
    const latY = (lat) => Y(((m.lat_north - lat) / (m.lat_north - m.lat_south)) * H);
    for (let lon = Math.ceil(m.lon_west / 10) * 10; lon <= m.lon_east; lon += 10) {
        if (lon === m.lon_west || lon === m.lon_east) continue;
        g.append(svg("line", { x1: lonX(lon), y1: Y(0), x2: lonX(lon), y2: Y(H), class: "bx-grat" }));
        g.append(svgText(lonX(lon), Y(H) + 16, -lon + "°W", "bx-small", "middle"));
    }
    for (let lat = Math.ceil(m.lat_south / 5) * 5; lat <= m.lat_north; lat += 5) {
        if (lat === m.lat_south || lat === m.lat_north) continue;
        g.append(svg("line", { x1: X(0), y1: latY(lat), x2: X(W), y2: latY(lat), class: "bx-grat" }));
        g.append(svgText(X(0) - 6, latY(lat) + 4, lat + "°N", "bx-small", "end"));
    }

    const wx = X(box.warning_x_cm);
    g.append(svg("line", { x1: wx, y1: Y(0.5), x2: wx, y2: Y(H - 0.5), class: "bx-warn" }));
    g.append(svgText(wx + 5, Y(1) + 12, "warning line: west of it is P", "bx-small bx-warn-t"));

    const t = box.target;
    g.append(svg("rect", {
        x: X(t.x_cm - t.size_cm / 2), y: Y(t.y_cm - t.size_cm / 2),
        width: t.size_cm * S, height: t.size_cm * S, class: "bx-target" }));
    g.append(svgText(X(t.x_cm), Y(t.y_cm + t.size_cm / 2) + 14, "target marker", "bx-small", "middle"));

    const nws = box.sites.find((s) => s.site === "NWS");
    const ais = box.sites.find((s) => s.site === "AIS");
    g.append(svg("line", { x1: X(nws.x_cm), y1: Y(nws.y_cm), x2: X(1), y2: Y(nws.y_cm), class: "bx-beam" }));
    g.append(svg("line", { x1: X(box.laser.x_cm), y1: Y(box.laser.y_cm), x2: X(ais.x_cm), y2: Y(ais.y_cm),
                           class: "bx-laser" }));
    g.append(svg("circle", { cx: X(box.laser.x_cm), cy: Y(box.laser.y_cm), r: 4, class: "bx-laser-dot" }));

    const LABEL = {
        NWS: { dx: -4, dy: 26, anchor: "end" },
        AUR: { dx: 12, dy: -2, anchor: "start" },
        AIS: { dx: 12, dy: -6, anchor: "start" },
        SAT: { dx: 14, dy: -8, anchor: "start" },
    };
    box.sites.forEach((site) => {
        const h = sensorFor(bench, site.site);
        const state = !h || h.ok === null ? "none" : h.ok ? "ok" : "not";
        const cx = X(site.x_cm), cy = Y(site.y_cm);
        if (site.site === "SAT") {
            g.append(svg("circle", { cx, cy, r: 13, class: "bx-pir" }));
        }
        g.append(svg("circle", { cx, cy, r: 7, class: "bx-site bx-" + state }));

        const L = LABEL[site.site];
        g.append(svgText(cx + L.dx, cy + L.dy, site.site + " · " + site.sensor, "bx-name", L.anchor));
        const r = READING[site.sensor];
        const reading = h && h.value !== null
            ? r.fmt(h.value) + "  (empty box " + r.fmt(h.base) + ")"
            : "no report";
        g.append(svgText(cx + L.dx, cy + L.dy + 15, reading, "bx-small", L.anchor));
    });

    return g;
}

export function renderBench(root, bench, box, prev, stalePolls) {
    root.replaceChildren();

    if (!box) {
        root.append(pending("No box geometry",
            "The drawing comes from data/box.json, and that file is missing.", null));
        return;
    }
    if (!bench) {
        root.append(pending("No bench status yet",
            "This view reads live/bench.json. make bench-health writes it with benchfeed, " +
            "from the TA's relay. With no board at all, make bench-sim runs a " +
            "simulated board and benchd on this machine.",
            "make bench-sim"));
        root.append(wiring(box));
        return;
    }

    const stale = stalePolls >= 3;
    const byBenchd = bench.source === "benchd";
    const board = bench.board || {};
    const hb = bench.heartbeat;

    const flags = [];
    if (stale) {
        flags.push(chip("bad", "bench.json has not changed for " + stalePolls + " s: " +
            (byBenchd ? "benchd" : "benchfeed") + " has stopped"));
    } else if (!board.connected) {
        flags.push(chip("bad", byBenchd ? "no board at " + board.device
                                        : "no heartbeat from the bench"));
    } else {
        flags.push(chip("ok", "board connected"));
    }
    if (hb && hb.clock === "none") {
        flags.push(chip("warn", "board clock not set: the centre refuses every contact as stale"));
    } else if (hb) {
        flags.push(chip("info", "clock set by benchd"));
    }
    if (hb && hb.dropped > 0) {
        flags.push(chip("warn", hb.dropped + " lines lost on the board"));
    }

    root.append(el("section", { class: "card", style: "margin-bottom:16px" },
        el("div", { class: "card-h" },
            el("h3", {}, text("The board")),
            el("span", { style: "flex:1" }),
            ...flags),
        el("div", { class: "card-b" },
            el("dl", { class: "stat-row" },
                stat("written by", byBenchd ? "benchd" : "benchfeed",
                     byBenchd ? board.device : board.via),
                stat("board up", hb ? hb.up_s + " s" : null, "since its last reset"),
                stat("heartbeat", hb ? hb.age_ms + " ms" : null, "ago, when the file was written"),
                stat("clock offset", hb && hb.clock === "host" ? hb.clock_offset_ms + " ms" : null,
                     "board minus this host"),
                byBenchd ? stat("lost, reopened", board.lost, "times the board went away") : null))));

    const rows = el("tbody", {});
    box.sites.forEach((site) => {
        const h = sensorFor(bench, site.site);
        const r = READING[site.sensor];
        const n = (bench.contacts || {})[site.site];
        let rate = "—";
        if (prev && prev.contacts && prev.written && bench.written > prev.written) {
            const d = (n - prev.contacts[site.site]) / (bench.written - prev.written);
            if (d >= 0) rate = d.toFixed(1);
        }
        rows.append(el("tr", {},
            el("td", {},
                el("div", {}, el("b", { class: "mono" }, text(site.site)),
                    text("  " + SITE_NAME[site.site])),
                el("small", { class: "muted" }, text(site.what + ", " + r.unit))),
            el("td", {}, okChip(h)),
            el("td", { class: "num mono" }, text(h && h.value !== null ? r.fmt(h.value) : "—")),
            el("td", { class: "num mono" }, text(h && h.base !== null ? r.fmt(h.base) : "—")),
            el("td", { class: "num mono" }, text(n === undefined ? "—" : n)),
            el("td", { class: "num mono" }, text(rate))));
    });
    const table = el("table", { class: "grid" },
        el("thead", {}, el("tr", {},
            el("th", {}, text("site and sensor")),
            el("th", {}, text("state")),
            el("th", { class: "num" }, text("now")),
            el("th", { class: "num" }, text("empty box")),
            el("th", { class: "num" }, text("plots")),
            el("th", { class: "num" }, text("plots/s")))),
        rows);

    root.append(el("section", { class: "card", style: "margin-bottom:16px" },
        el("div", { class: "card-h" },
            el("h3", {}, text("The box")),
            el("span", { style: "flex:1" }),
            chip("info", "geometry from data/box.json")),
        el("div", { class: "card-b bench-layout" },
            el("div", { class: "bx-wrap" }, drawBox(box, bench, stale)),
            el("div", {},
                el("div", { class: "tablewrap" }, table),
                el("p", { class: "note" }, text(
                    "The board decides whether a sensor sees something, against its empty-box " +
                    "reading, and says so only by sending plots. A reading that differs from " +
                    "the empty box with no plots is inside the margin the firmware allows."))))));

    const stored = hb ? hb.sat_stored : null;
    const pct = stored === null ? 0 : Math.min(100, (stored / 2000) * 100);
    root.append(el("section", { class: "card", style: "margin-bottom:16px" },
        el("div", { class: "card-h" },
            el("h3", {}, text("The satellite's store")),
            el("span", { style: "flex:1" }),
            chip("info", "store and forward")),
        el("div", { class: "card-b" },
            el("div", { class: "store" },
                el("div", { class: "store-fill", style: "width:" + pct.toFixed(1) + "%" })),
            el("p", { class: "note" }, text(
                (stored === null ? "No heartbeat yet." : stored + " of 2000 records held. ") +
                "The board sends them all in one burst when it holds 2000, or when the oldest " +
                "is " + box.sat_max_age_s + " s old: the centre refuses a report older than 60 s, " +
                "so a record held longer would arrive only to be refused.")))));

    const lines = bench.lines || {};
    const counts = [
        stat("lines", lines.total),
        stat("heartbeats", lines.heartbeat),
        stat("health", lines.health),
        stat("rejected", lines.rejected, "did not parse"),
        stat("unknown", lines.unknown, "a drill's text, or damage"),
    ];
    if (byBenchd) {
        counts.push(stat("too long", board.too_long, "lost, and the reader recovered"));
        counts.push(stat("clock lines sent", board.clock_sent));
    }
    const cards = [el("section", { class: "card" },
        el("div", { class: "card-h" }, el("h3", {}, text("Lines from the board"))),
        el("div", { class: "card-b" }, el("dl", { class: "stat-row" }, ...counts)))];

    if (byBenchd && bench.relay) {
        cards.push(el("section", { class: "card" },
            el("div", { class: "card-h" },
                el("h3", {}, text("The relay")),
                el("span", { style: "flex:1" }),
                chip("info", "port " + bench.relay.port)),
            el("div", { class: "card-b" },
                el("dl", { class: "stat-row" },
                    stat("consumers", bench.relay.consumers, "SENTINELs subscribed"),
                    stat("dropped slow", bench.relay.dropped_slow, "stopped reading"),
                    stat("refused", bench.relay.refused, "the relay was full")))));
    }
    root.append(el("div", { class: "bench-cards", style: "margin-bottom:16px" }, ...cards));

    root.append(wiring(box));
}

function wiring(box) {
    const tb = el("tbody", {});
    (box.pins || []).forEach((p) => {
        tb.append(el("tr", {},
            el("td", {}, text(p.what)),
            el("td", { class: "mono" }, text("GP" + p.gpio)),
            el("td", { class: "num mono" }, text(p.pin)),
            el("td", {}, text(p.row)),
            el("td", {}, chip(p.rail === "5V" ? "warn" : "info", p.rail)),
            el("td", {}, text(p.safe))));
    });
    return el("section", { class: "card" },
        el("div", { class: "card-h" },
            el("h3", {}, text("Wiring")),
            el("span", { style: "flex:1" }),
            chip("info", "from data/box.json")),
        el("div", { class: "card-b tablewrap" },
            el("table", { class: "grid" },
                el("thead", {}, el("tr", {},
                    el("th", {}, text("signal")),
                    el("th", {}, text("GPIO")),
                    el("th", { class: "num" }, text("Pico pin")),
                    el("th", {}, text("breadboard")),
                    el("th", {}, text("sensor rail")),
                    el("th", {}, text("why the pin stays at 3.3 V")))),
                tb),
            el("p", { class: "note" }, text(
                "5 V sensors on the left half, 3.3 V on the right. Measure every signal in a " +
                "parking row before it goes into a Pico row."))));
}
