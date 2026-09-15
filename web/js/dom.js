export function el(tag, attrs, ...children) {
    const node = document.createElement(tag);

    for (const [key, value] of Object.entries(attrs || {})) {
        if (value === null || value === undefined || value === false) continue;
        if (key.startsWith("on") && typeof value === "function") {
            node.addEventListener(key.slice(2), value);
        } else if (key === "class") {
            node.className = value;
        } else {
            node.setAttribute(key, value === true ? "" : String(value));
        }
    }

    for (const child of children) {
        if (child === null || child === undefined || child === false) continue;
        node.append(child);
    }
    return node;
}

export function text(value) {
    return document.createTextNode(value === null || value === undefined ? "" : String(value));
}

export function chip(kind, label) {
    return el("span", { class: "chip " + kind }, text(label));
}

export function pending(title, detail, command) {
    return el("div", { class: "pending" },
        el("strong", {}, text(title)),
        detail ? el("p", { style: "margin:0;max-width:52ch" }, text(detail)) : null,
        command ? el("p", { style: "margin:4px 0 0" },
            text("Run "), el("code", {}, text(command))) : null);
}
