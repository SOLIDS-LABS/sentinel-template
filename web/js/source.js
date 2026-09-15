function unavailable(reason, needs) {
    return { ok: false, reason: reason, needs: needs || null, lines: [], offset: 0 };
}

function available(lines, offset) {
    return { ok: true, reason: null, needs: null, lines: lines, offset: offset };
}

class Source {
    constructor(options) {
        this.options = options || {};
        this.name = "source";
    }

    async box() {
        try {
            const res = await fetch(this.options.boxUrl || "data/box.json", { cache: "no-store" });
            return res.ok ? { ok: true, reason: null, needs: null, data: await res.json() }
                          : unavailable("no data/box.json", null);
        } catch (e) {
            return unavailable("cannot read data/box.json", null);
        }
    }

    async bench()    { return unavailable("no bench status", "make bench-sim"); }
    async events()   { return unavailable("no mission log", "make lab1"); }
    async contacts() { return unavailable("no contact feed", "make lab1"); }
    async run()      { return unavailable("no run recorded", null); }
    async tracks()   { return unavailable("no track picture", null); }
    async live()     { return unavailable("no live run", null); }
    async compare()  { return unavailable("no comparison", "make compare"); }
}

class FileSource extends Source {
    constructor(options) {
        super(options);
        this.name = "file";
        this.root = (options && options.root) || "live";
    }

    url(name) { return this.root + "/" + name; }

    async tail(name, since, waitHint) {
        const from = since > 0 ? since : 0;
        let res;
        try {
            res = await fetch(this.url(name), {
                cache: "no-store",
                headers: from > 0 ? { Range: "bytes=" + from + "-" } : {},
            });
        } catch (e) {
            return unavailable("cannot reach the server", waitHint);
        }

        if (res.status === 416) {
            const range = res.headers.get("Content-Range") || "";
            const found = /\/\s*(\d+)\s*$/.exec(range);
            const size  = found ? Number(found[1]) : null;
            if (size !== null && size >= from) return available([], from);
            return available([], 0);
        }
        if (res.status === 404) return unavailable(this.url(name) + " does not exist", waitHint);
        if (!res.ok) return unavailable(this.url(name) + " returned " + res.status, waitHint);

        const text = await res.text();

        if (res.status === 204) return available([], from);

        let fresh = text;
        if (from > 0 && res.status !== 206) fresh = text.slice(from);

        const offset = from + new TextEncoder().encode(fresh).length;
        const lines = fresh.split("\n").filter((l) => l.length > 0);
        return available(lines, offset);
    }

    async events(since)   { return this.tail("missions.log", since, "make lab1"); }
    async contacts(since) { return this.tail("feed.txt", since, "make lab1"); }

    async run() {
        return this.json("run.json", null, "no run recorded yet");
    }

    async compare() {
        return this.json("compare.json", null, "no comparison yet");
    }

    async tracks() {
        return this.json("tracks.json", null, "no track picture yet");
    }

    async live() {
        return this.json("live.json", null, "no live run yet: a later lab publishes it");
    }

    async bench() {
        return this.json("bench.json", "make bench-sim", "no bench.json yet");
    }

    async json(name, waitHint, missing) {
        let res;
        try {
            res = await fetch(this.url(name), { cache: "no-store" });
        } catch (e) {
            return unavailable("cannot reach the server", waitHint);
        }
        if (!res.ok) return unavailable(missing, waitHint);
        try {
            return { ok: true, reason: null, needs: null,
                     data: await res.json() };
        } catch (e) {
            return unavailable(name + " is not valid JSON", waitHint);
        }
    }
}

class AgentSource extends Source {
    constructor(options) {
        super(options);
        this.name = "agent";
        this.host = (options && options.host) || "";
    }

    base() { return /^https?:\/\//.test(this.host) ? this.host : "http://" + this.host; }

    async ask(path, since) {
        if (!this.host) {
            return unavailable("no host named", "add ?source=agent&host=<ip>:8080");
        }
        const url = this.base() + path + (since ? "?since=" + since : "");
        let res;
        try {
            res = await fetch(url, { cache: "no-store" });
        } catch (e) {
            return unavailable("cannot reach " + this.host, "no agent is built: use the file source");
        }
        if (!res.ok) return unavailable(path + " returned " + res.status, null);
        const body = await res.json();
        return available(body.lines || [], body.offset || 0);
    }

    async events(since)   { return this.ask("/events", since); }
    async contacts(since) { return this.ask("/contacts", since); }
    async run()           { return this.ask("/run", 0); }
    async tracks()        { return this.ask("/tracks", 0); }
    async bench()         { return this.ask("/bench", 0); }
}

class ReplaySource extends Source {
    constructor(options) {
        super(options);
        this.name = "replay";
        this.file = (options && options.replay) || "data/replay.log";
        this.cache = null;
    }

    async load() {
        if (this.cache) return this.cache;
        const res = await fetch(this.file, { cache: "no-store" });
        this.cache = res.ok
            ? (await res.text()).split("\n").filter((l) => l.length > 0)
            : null;
        return this.cache;
    }

    async events(since) {
        const all = await this.load();
        if (!all) return unavailable("no recording in " + this.file, "make lab1");
        const from = since > 0 ? since : 0;
        return available(all.slice(from), all.length);
    }

    async contacts() { return unavailable("a recording holds no feed", "make lab1"); }
}

function sourceFromLocation(search) {
    const q = new URLSearchParams(search || window.location.search);
    const want = (q.get("source") || "file").toLowerCase();
    const options = {
        host: q.get("host") || "",
        root: q.get("live") || "live",
        replay: q.get("replay") || "data/replay.log",
    };

    if (want === "agent")  return new AgentSource(options);
    if (want === "replay") return new ReplaySource(options);
    return new FileSource(options);
}

export {
    Source, FileSource, AgentSource, ReplaySource,
    sourceFromLocation, available, unavailable,
};
