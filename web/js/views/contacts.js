import { el, text, chip, pending } from "../dom.js";
import { contactsForDisplay } from "../model.js";
import { clockText } from "../parse.js";

function deg(value, neg, pos) {
    if (!isFinite(value)) return "—";
    return Math.abs(value).toFixed(4) + "° " + (value < 0 ? neg : pos);
}

export function renderContacts(root, model, tracks) {
    root.replaceChildren();

    let rows = contactsForDisplay(model);
    let fromTracks = false;

    if (!rows.length && tracks && tracks.tracks && tracks.tracks.length) {
        rows = tracks.tracks.map((t) => ({
            id: t.id, sensor: t.sensor, lat: t.lat, lon: t.lon, speed: t.speed,
            priority: !!t.priority, reports: t.reports, stamp: null,
        }));
        fromTracks = true;
    }

    if (!rows.length) {
        root.append(pending(
            "No contact reports yet",
            "The page is watching web/live/feed.txt, which holds the lat, lon and " +
            "speed of every contact. Part 1 writes that file. From Part 2 on, the " +
            "sensors write down a pipe instead, and this view shows the track " +
            "picture that the centre publishes.",
            "make lab1"));
        return;
    }

    const fromLog = !fromTracks && rows.every((c) => c.fromLog);
    const priority = rows.filter((c) => c.priority).length;

    const head = el("div", { class: "card-h" },
        el("h3", {}, text(fromTracks ? "Track picture" : "Contact reports")),
        el("span", { style: "flex:1" }),
        chip("info", rows.length + (fromTracks ? " tracks" : " contacts")),
        chip("warn", priority + " priority"));

    const table = el("table", { class: "grid" });
    table.append(el("thead", {}, el("tr", {},
        el("th", {}, text(fromTracks ? "reports" : "time")),
        el("th", {}, text("sensor")),
        el("th", {}, text("contact")),
        el("th", { class: "num" }, text("latitude")),
        el("th", { class: "num" }, text("longitude")),
        el("th", { class: "num" }, text("speed")),
        el("th", {}, text("flag")))));

    const tb = el("tbody", {});
    rows.slice(-600).forEach((c) => {
        tb.append(el("tr", { class: c.priority ? "is-priority" : null },
            el("td", { class: "mono dim" },
               text(fromTracks ? String(c.reports) : clockText(c.stamp))),
            el("td", { class: "mono" }, text(c.sensor)),
            el("td", { class: "mono" }, text(c.id)),
            el("td", { class: "num mono" }, text(deg(c.lat, "S", "N"))),
            el("td", { class: "num mono" }, text(deg(c.lon, "W", "E"))),
            el("td", { class: "num mono" }, text(c.speed.toFixed(1) + " kn")),
            el("td", {}, chip(c.priority ? "warn" : "idle", c.priority ? "P" : "R"))));
    });
    table.append(tb);

    root.append(el("section", { class: "card" }, head,
        el("div", { class: "card-b tablewrap" }, table)));

    if (fromTracks) {
        root.append(el("p", { class: "note" }, text(
            "These are TRACKS, one for each target, from tracks.json. Part 2 and " +
            "later carry every report down a pipe and write no feed file, so there " +
            "is no per-report list to show. Three sensors watching one aircraft are " +
            "ONE row here, and the first column is how many reports made it.")));
    }
    if (fromLog) {
        root.append(el("p", { class: "note" }, text(
            "These came from the mission log, which carries a position only for a " +
            "priority contact. For every contact, send the feed to web/live/feed.txt.")));
    }
    if (rows.length > 600) {
        root.append(el("p", { class: "note" },
            text("Showing the last 600 of " + rows.length + ".")));
    }
    if (model.contactBad.length) {
        root.append(el("p", { class: "note bad" }, text(
            model.contactBad.length + " line(s) did not parse. First: " +
            model.contactBad[0].why)));
    }
}
