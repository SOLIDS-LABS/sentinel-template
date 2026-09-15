import { el, text, chip, pending } from "../dom.js";
import { LEVELS, clockText } from "../parse.js";

const LEVEL_CLASS = {
    SYSTEM:   "info",
    INFO:     "idle",
    PENDING:  "warn",
    PRIORITY: "warn",
    ALERT:    "bad",
    TRACK:    "ok",
};

let filter = "ALL";

const PAGE = 500;

let page = null;

export function renderLog(root, model, waiting) {
    root.replaceChildren();

    if (!model.events.length) {
        root.append(pending(
            "No mission log yet",
            waiting || "The page is watching web/live/missions.log. Your code writes it " +
                "when log_open and log_writef work.",
            "make lab1"));
        return;
    }

    const bar = el("div", { class: "filter-bar" });
    const add = (name, count, cls) => {
        bar.append(el("button", {
            class: "filter" + (filter === name ? " on" : ""),
            onclick: () => {
                filter = name;
                page   = null;
                renderLog(root, model, waiting);
            },
        },
            el("span", { class: "dot " + (cls || "idle") }),
            text(name),
            el("b", {}, text(String(count)))));
    };
    add("ALL", model.events.length, null);
    LEVELS.forEach((l) => { if (model.levelCount[l]) add(l, model.levelCount[l], LEVEL_CLASS[l]); });
    if (model.unparsed) add("UNPARSED", model.unparsed, "bad");

    const rows = model.events.filter((e) => {
        if (filter === "ALL") return true;
        if (filter === "UNPARSED") return !e.ok;
        return e.level === filter;
    });

    const pages = Math.max(1, Math.ceil(rows.length / PAGE));
    const here  = (page === null) ? pages : Math.min(Math.max(page, 1), pages);
    const from  = (here - 1) * PAGE;
    const shown = rows.slice(from, from + PAGE);

    const body = el("div", { class: "logbox" });
    shown.forEach((e) => {
        if (!e.ok) {
            body.append(el("div", { class: "logline bad" },
                el("span", { class: "lv bad" }, text("?")),
                el("span", { class: "msg" }, text(e.raw))));
            return;
        }
        body.append(el("div", { class: "logline" },
            el("span", { class: "ts" }, text(clockText(e.stamp))),
            el("span", { class: "lv " + (LEVEL_CLASS[e.level] || "idle") }, text(e.level)),
            el("span", { class: "msg" }, text(e.message))));
    });

    const pager = el("div", { class: "filter-bar" });
    const step = (label, to, on) => {
        pager.append(el("button", {
            class: "filter" + (on ? "" : " off"),
            disabled: on ? null : "disabled",
            onclick: () => {
                if (!on) return;
                page = to;
                renderLog(root, model, waiting);
            },
        }, text(label)));
    };

    step("« oldest", 1, here > 1);
    step("‹ older", here - 1, here > 1);
    pager.append(el("span", { class: "note", style: "padding:0 0.7em" },
        text("page " + here + " of " + pages + ", lines " +
             (rows.length ? from + 1 : 0) + " to " + (from + shown.length) +
             " of " + rows.length)));
    step("newer ›", here + 1, here < pages);
    step("newest »", null, here < pages);

    root.append(el("section", { class: "card" },
        el("div", { class: "card-h" },
            el("h3", {}, text("Mission log")),
            el("span", { style: "flex:1" }),
            chip("ok", "R6  every action timestamped")),
        el("div", { class: "card-b" }, bar, body, pager)));

    if (model.eventsDropped) {
        root.append(el("p", { class: "note" },
            text(model.eventsDropped + " earlier lines are no longer held. " +
                 "The counts above are totals for the whole run, not for the " +
                 "lines kept. No ALERT is ever dropped.")));
    }
}
