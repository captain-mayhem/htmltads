/*
 *   guit3 WebUI bridge - the page half of the WebUI-over-Emscripten transport
 *   (webui-emscripten-plan.md, step 4). Loaded by guit3.html, next to guit3.js.
 *
 *   guit3-webui-sw.js (the Service Worker) catches a WebUI page's requests and
 *   posts each one here as plain data. This file:
 *
 *   - serializes it into HTTP/1.1 request bytes and pushes them into the VM
 *     over a loopback connection (tads3/emscripten/osnetloop.h's osu_loop_*()
 *     API - the game's HTTPServer sees an ordinary socket);
 *   - parses the VM's raw reply bytes (status line, headers, then a
 *     Content-Length, chunked, or read-to-close body) and streams them back
 *     to the worker as "head"/"chunk"/"end" messages, so the long-poll
 *     /webui/getEvent request and file downloads stream rather than buffer;
 *   - keeps a small pool of keep-alive loopback connections. Each open
 *     connection costs the VM one server thread, taken from guit3's fixed
 *     pthread pool (see PTHREAD_POOL_SIZE in imgui/CMakeLists.txt), so the
 *     pool is capped and idle connections are closed again after a while;
 *   - keeps the cookie jar. The WebUI library identifies its session and
 *     client with cookies (TADS_session/TADS_client, lib/webui.t), but a
 *     Service Worker can neither see a request's Cookie header nor set
 *     cookies from a synthesized response's Set-Cookie, so the bridge stores
 *     Set-Cookie values itself and adds a Cookie header to each request. The
 *     jar lives exactly as long as the VM it talks to, which is the right
 *     lifetime for these session cookies anyway;
 *   - holds the durable copy of the worker's routing table for this VM
 *     (step 5: which documents this VM served, so the worker can route
 *     their requests here - see "Routing" in guit3-webui-sw.js).
 *
 *   Nothing here runs until the game actually starts a WebUI session: the
 *   loopback port comes from Module.webuiLaunch, which guit3's Emscripten
 *   launch hook (guit3_webui_launch_hook(), guit3.cpp) publishes, and whose
 *   existence also proves the wasm runtime is initialized, so native calls
 *   are safe.
 */

"use strict";

var Guit3WebUIBridge = (function () {

    /* concurrent loopback connections (= VM server threads) we'll open */
    const MAX_CONNS = 3;

    /* close a keep-alive connection that's been idle this long, freeing its server thread */
    const IDLE_CLOSE_MS = 5000;

    /* how long a request may wait for the game's WebUI server to appear */
    const LAUNCH_WAIT_MS = 30000;

    /* how often to poll the transport for reply bytes while anything is in flight */
    const POLL_MS = 4;

    /* how often to re-announce our routing table, in case the worker was restarted */
    const ANNOUNCE_MS = 5000;

    /* how many served documents/clients to remember for routing (oldest dropped first) */
    const ROUTE_MAX = 64;

    /* size of the scratch buffer replies are pulled through */
    const PULL_CHUNK = 65536;

    /* request headers we never forward as-is: hop-by-hop, or set by us */
    const DROP_REQ_HEADERS = new Set([
        "host", "connection", "keep-alive", "content-length",
        "transfer-encoding", "cookie", "te", "upgrade", "proxy-connection",
    ]);

    /* reply headers we never hand back to the Response */
    const DROP_REPLY_HEADERS = new Set([
        "connection", "keep-alive", "transfer-encoding", "set-cookie", "set-cookie2",
    ]);

    let loop = null;            /* cwrap'd osu_loop_*() functions, once available */
    let scratchPtr = 0;         /* PULL_CHUNK-byte wasm heap buffer for pulls */

    const queue = [];           /* jobs waiting for a connection */
    const conns = [];           /* open loopback connections */
    let pumpTimer = null;

    const cookies = new Map();  /* "name|path" -> { name, value, path } */

    /* the last few requests, for debugging from the console: Guit3WebUIBridge.recent() */
    const RECENT_MAX = 50;
    const recentLog = [];

    let worker = null;          /* the active Service Worker, once ready */

    /*
     *   Our half of the worker's routing table (see "Routing" in
     *   guit3-webui-sw.js): the client ids and URLs of every document this
     *   VM has served, as the worker reports them. The worker only caches
     *   this - it loses everything when the browser restarts it - so this
     *   page is where it survives, and every announce()/ping reply carries
     *   it back.
     */
    const servedClients = [];
    const servedDocs = [];
    let routed = Promise.resolve();     /* see whenRouted() */

    /* ------------------------------------------------------------------ */
    /* wasm access */

    function launchInfo() {
        return (typeof Module !== "undefined" && Module.webuiLaunch) || null;
    }

    function ensureLoop() {
        if (loop)
            return true;
        if (!launchInfo())
            return false;
        loop = {
            newConn: Module.cwrap("osu_loop_new_conn", "number", ["number"]),
            push: Module.cwrap("osu_loop_push", "number", ["number", "number", "number"]),
            pendingLen: Module.cwrap("osu_loop_pending_len", "number", ["number"]),
            pull: Module.cwrap("osu_loop_pull", "number", ["number", "number", "number"]),
            isClosed: Module.cwrap("osu_loop_is_closed", "number", ["number"]),
            endConn: Module.cwrap("osu_loop_end_conn", null, ["number"]),
        };
        scratchPtr = Module._malloc(PULL_CHUNK);
        return true;
    }

    /*
     *   A current view of the wasm heap. With pthreads plus memory growth,
     *   another thread can grow memory without Module.HEAPU8 being replaced,
     *   leaving it too short; the runtime's own helpers refresh the exported
     *   views (updateMemoryViews()) before touching the heap, so running one
     *   of them - a zero-length UTF8ToString() - brings HEAPU8 up to date.
     */
    function heap(ptr) {
        Module.UTF8ToString(ptr, 0);
        return Module.HEAPU8;
    }

    /* ------------------------------------------------------------------ */
    /* cookies */

    function defaultCookiePath(reqPath) {
        const i = reqPath.lastIndexOf("/");
        return i <= 0 ? "/" : reqPath.slice(0, i);
    }

    function storeCookie(setCookie, reqPath) {
        const parts = setCookie.split(";");
        const eq = parts[0].indexOf("=");
        if (eq <= 0)
            return;
        const name = parts[0].slice(0, eq).trim();
        const value = parts[0].slice(eq + 1).trim();
        let path = null, expired = false;
        for (const attr of parts.slice(1)) {
            const aeq = attr.indexOf("=");
            const key = (aeq < 0 ? attr : attr.slice(0, aeq)).trim().toLowerCase();
            const val = aeq < 0 ? "" : attr.slice(aeq + 1).trim();
            if (key === "path" && val.startsWith("/"))
                path = val;
            else if (key === "max-age" && parseInt(val, 10) <= 0)
                expired = true;
            else if (key === "expires" && Date.parse(val) < Date.now())
                expired = true;
        }
        if (path === null)
            path = defaultCookiePath(reqPath);
        const key = name + "|" + path;
        if (expired)
            cookies.delete(key);
        else
            cookies.set(key, { name, value, path });
    }

    function cookieHeader(reqPath) {
        const matches = [];
        for (const c of cookies.values()) {
            if (reqPath === c.path
                || (reqPath.startsWith(c.path)
                    && (c.path.endsWith("/") || reqPath[c.path.length] === "/")))
                matches.push(c);
        }
        /* longer paths first, per RFC 6265 */
        matches.sort((a, b) => b.path.length - a.path.length);
        return matches.map((c) => c.name + "=" + c.value).join("; ");
    }

    /* ------------------------------------------------------------------ */
    /* HTTP serialization */

    /* header text is ISO-8859-1 on the wire; anything wider can't be sent as-is */
    function latin1Bytes(s) {
        const out = new Uint8Array(s.length);
        for (let i = 0; i < s.length; ++i) {
            const c = s.charCodeAt(i);
            out[i] = c < 256 ? c : 0x3F;
        }
        return out;
    }

    function latin1String(u8) {
        let s = "";
        for (let i = 0; i < u8.length; i += 8192)
            s += String.fromCharCode.apply(null, u8.subarray(i, i + 8192));
        return s;
    }

    function serializeRequest(job, port) {
        const reqPath = job.target.split("?")[0];
        let head = job.method + " " + job.target + " HTTP/1.1\r\n"
            + "Host: 127.0.0.1:" + port + "\r\n";
        for (const [k, v] of job.headers) {
            if (!DROP_REQ_HEADERS.has(k.toLowerCase()))
                head += k + ": " + v + "\r\n";
        }
        const ck = cookieHeader(reqPath);
        if (ck)
            head += "Cookie: " + ck + "\r\n";
        if (job.body !== null)
            head += "Content-Length: " + job.body.byteLength + "\r\n";
        head += "Connection: keep-alive\r\n\r\n";

        const hb = latin1Bytes(head);
        if (job.body === null || job.body.byteLength === 0)
            return hb;
        const out = new Uint8Array(hb.length + job.body.byteLength);
        out.set(hb, 0);
        out.set(new Uint8Array(job.body), hb.length);
        return out;
    }

    /* ------------------------------------------------------------------ */
    /* HTTP reply parsing */

    function concat(a, b) {
        if (a.length === 0)
            return b;
        const out = new Uint8Array(a.length + b.length);
        out.set(a, 0);
        out.set(b, a.length);
        return out;
    }

    /* offset just past the first CRLF at or after 'from', or -1 */
    function findCRLF(buf, from) {
        for (let i = from; i + 1 < buf.length; ++i) {
            if (buf[i] === 13 && buf[i + 1] === 10)
                return i + 2;
        }
        return -1;
    }

    /* offset just past the first CRLFCRLF, or -1 */
    function findHeadEnd(buf) {
        for (let i = 0; i + 3 < buf.length; ++i) {
            if (buf[i] === 13 && buf[i + 1] === 10 && buf[i + 2] === 13 && buf[i + 3] === 10)
                return i + 4;
        }
        return -1;
    }

    /*
     *   Incremental parser for one reply. feed() takes bytes as they're
     *   pulled; the callbacks receive the parsed head, body data, and the end
     *   of the reply. 'state' ends as "done" (reply complete) or "error".
     *   reusable is true once done if the connection can carry another
     *   request.
     */
    class ReplyParser {
        constructor(isHead, cb) {
            this.isHead = isHead;
            this.cb = cb;
            this.state = "head";
            this.buf = new Uint8Array(0);
            this.remaining = 0;
            this.reusable = true;
            this.extra = false;     /* bytes arrived after the reply ended */
        }

        feed(bytes) {
            if (this.state === "done" || this.state === "error") {
                if (bytes.length)
                    this.extra = true;
                return;
            }
            this.buf = concat(this.buf, bytes);
            while (this.step()) ;
        }

        /* the VM closed its end, and every byte it sent has been fed */
        eof() {
            if (this.state === "close") {
                this.finish();
                return;
            }
            this.reusable = false;
            if (this.state !== "done")
                this.fail("the game closed the connection before the reply was complete");
        }

        finish() {
            this.state = "done";
            this.cb.onEnd();
            if (this.buf.length)
                this.extra = true;
        }

        fail(msg) {
            this.state = "error";
            this.reusable = false;
            this.cb.onError(msg);
        }

        /* advance by one unit of work; returns true if it made progress */
        step() {
            const buf = this.buf;
            switch (this.state) {
            case "head": {
                const end = findHeadEnd(buf);
                if (end < 0)
                    return false;
                this.buf = buf.slice(end);
                this.parseHead(latin1String(buf.subarray(0, end - 4)));
                return true;
            }
            case "length": {
                if (buf.length === 0)
                    return false;
                const n = Math.min(this.remaining, buf.length);
                this.cb.onData(buf.slice(0, n));
                this.buf = buf.slice(n);
                if ((this.remaining -= n) === 0)
                    this.finish();
                return this.state !== "done";
            }
            case "close":
                if (buf.length === 0)
                    return false;
                this.cb.onData(buf);
                this.buf = new Uint8Array(0);
                return false;
            case "chunk-size": {
                const end = findCRLF(buf, 0);
                if (end < 0)
                    return false;
                const size = parseInt(latin1String(buf.subarray(0, end - 2)), 16);
                this.buf = buf.slice(end);
                if (isNaN(size) || size < 0) {
                    this.fail("malformed chunk size in the game's reply");
                    return false;
                }
                if (size === 0) {
                    this.state = "trailer";
                } else {
                    this.remaining = size;
                    this.state = "chunk-data";
                }
                return true;
            }
            case "chunk-data": {
                if (buf.length === 0)
                    return false;
                const n = Math.min(this.remaining, buf.length);
                this.cb.onData(buf.slice(0, n));
                this.buf = buf.slice(n);
                if ((this.remaining -= n) === 0)
                    this.state = "chunk-crlf";
                return true;
            }
            case "chunk-crlf":
                if (buf.length < 2)
                    return false;
                this.buf = buf.slice(2);
                this.state = "chunk-size";
                return true;
            case "trailer": {
                /* trailer lines until an empty line; trailers themselves are dropped */
                const end = findCRLF(buf, 0);
                if (end < 0)
                    return false;
                this.buf = buf.slice(end);
                if (end === 2)
                    this.finish();
                return this.state !== "done";
            }
            default:
                return false;
            }
        }

        parseHead(text) {
            const lines = text.split("\r\n");
            const m = /^HTTP\/1\.([01])\s+(\d{3})\s*(.*)$/.exec(lines[0]);
            if (!m) {
                this.fail("malformed status line in the game's reply: " + lines[0]);
                return;
            }
            const status = parseInt(m[2], 10);

            /* an interim 1xx reply is followed by the real one */
            if (status >= 100 && status < 200) {
                this.state = "head";
                return;
            }

            const headers = [];
            let contentLength = null, chunked = false, connClose = m[1] === "0";
            for (const line of lines.slice(1)) {
                const colon = line.indexOf(":");
                if (colon <= 0)
                    continue;
                const name = line.slice(0, colon).trim();
                const value = line.slice(colon + 1).trim();
                const lname = name.toLowerCase();
                if (lname === "set-cookie")
                    this.cb.onCookie(value);
                else if (lname === "content-length")
                    contentLength = parseInt(value, 10);
                else if (lname === "transfer-encoding")
                    chunked = /chunked/i.test(value);
                else if (lname === "connection")
                    connClose = /close/i.test(value) ? true
                        : /keep-alive/i.test(value) ? false : connClose;
                if (!DROP_REPLY_HEADERS.has(lname))
                    headers.push([name, value]);
            }
            if (connClose)
                this.reusable = false;

            this.cb.onHead(status, m[3], headers);

            if (this.isHead || status === 204 || status === 304) {
                this.finish();
            } else if (chunked) {
                this.state = "chunk-size";
            } else if (contentLength !== null && !isNaN(contentLength)) {
                this.remaining = contentLength;
                if (contentLength === 0)
                    this.finish();
                else
                    this.state = "length";
            } else if (connClose) {
                this.state = "close";
            } else {
                /*
                 *   No length, not chunked, and the connection stays open:
                 *   the VM only does this for a reply with no body at all
                 *   (see vmhttpreq.cpp's reply sender - Content-Length is
                 *   sent whenever there's a body).
                 */
                this.finish();
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* jobs and connections */

    function post(job, msg, transfer) {
        try {
            job.port.postMessage(msg, transfer || []);
        } catch (e) {
            /* the worker side went away - nothing left to tell */
        }
    }

    function failJob(job, message) {
        if (job.finished)
            return;
        job.finished = true;
        post(job, { type: "error", message });
        job.port.close();
    }

    function acceptRequest(msg, port) {
        const job = {
            method: msg.method,
            target: msg.target,
            headers: msg.headers,
            body: msg.body,
            port,
            queuedAt: Date.now(),
            conn: null,
            finished: false,
            log: { method: msg.method, target: msg.target, status: null, headMs: null },
        };
        recentLog.push(job.log);
        if (recentLog.length > RECENT_MAX)
            recentLog.shift();
        port.onmessage = (e) => {
            if (e.data && e.data.type === "cancel")
                cancelJob(job);
        };
        queue.push(job);
        schedulePump(0);
    }

    function cancelJob(job) {
        if (job.finished)
            return;
        job.finished = true;
        job.port.close();
        const qi = queue.indexOf(job);
        if (qi >= 0)
            queue.splice(qi, 1);
        if (job.conn) {
            /* mid-reply - the connection can't carry anything else */
            dropConn(job.conn);
        }
    }

    function dropConn(conn) {
        const i = conns.indexOf(conn);
        if (i >= 0)
            conns.splice(i, 1);
        if (conn.job)
            conn.job.conn = null;
        conn.job = null;
        loop.endConn(conn.id);
    }

    /* push a whole byte array into the VM through a temporary heap buffer */
    function pushBytes(connId, bytes) {
        const ptr = Module._malloc(bytes.length);
        try {
            heap(ptr).set(bytes, ptr);
            return loop.push(connId, ptr, bytes.length);
        } finally {
            Module._free(ptr);
        }
    }

    function startJob(job, conn) {
        conn.job = job;
        job.conn = conn;
        const reqPath = job.target.split("?")[0];
        job.parser = new ReplyParser(job.method === "HEAD", {
            onHead(status, statusText, headers) {
                job.log.status = status;
                job.log.headMs = Date.now() - job.queuedAt;
                post(job, { type: "head", status, statusText, headers });
            },
            onData(bytes) {
                post(job, { type: "chunk", bytes: bytes.buffer }, [bytes.buffer]);
            },
            onEnd() {
                job.finished = true;
                post(job, { type: "end" });
                job.port.close();
            },
            onError(message) {
                failJob(job, message);
            },
            onCookie(value) {
                storeCookie(value, reqPath);
            },
        });
        const bytes = serializeRequest(job, conn.port);
        job.body = null;
        if (pushBytes(conn.id, bytes) < 0) {
            failJob(job, "could not send the request to the game");
            dropConn(conn);
        }
    }

    /* an idle connection still usable for a request to the given port, or null */
    function takeIdleConn(port) {
        for (const conn of conns.slice()) {
            if (conn.job)
                continue;
            if (conn.port !== port || loop.isClosed(conn.id) || loop.pendingLen(conn.id) > 0) {
                dropConn(conn);
                continue;
            }
            return conn;
        }
        return null;
    }

    function assignJobs() {
        const launch = launchInfo();
        while (queue.length) {
            const job = queue[0];
            if (!launch || !ensureLoop()) {
                if (Date.now() - job.queuedAt > LAUNCH_WAIT_MS) {
                    queue.shift();
                    failJob(job, "the game has not started its WebUI server");
                    continue;
                }
                return;
            }
            let conn = takeIdleConn(launch.port);
            if (!conn) {
                if (conns.length >= MAX_CONNS)
                    return;     /* wait for a connection to free up */
                const id = loop.newConn(launch.port);
                if (id < 0) {
                    queue.shift();
                    failJob(job, "the game's WebUI server is not listening");
                    continue;
                }
                conn = { id, port: launch.port, job: null, idleSince: 0 };
                conns.push(conn);
            }
            queue.shift();
            startJob(job, conn);
        }
    }

    /* pull everything the VM has sent on this connection into its reply parser */
    function service(conn) {
        const job = conn.job;
        /* sample closed-ness first, so no bytes sent before the close can be missed */
        const closed = loop.isClosed(conn.id);
        for (;;) {
            const n = loop.pull(conn.id, scratchPtr, PULL_CHUNK);
            if (n <= 0)
                break;
            job.parser.feed(heap(scratchPtr).slice(scratchPtr, scratchPtr + n));
        }
        const p = job.parser;
        if (p.state !== "done" && p.state !== "error" && closed)
            p.eof();
        if (p.state === "done" || p.state === "error") {
            if (p.state === "done" && p.reusable && !p.extra && !closed) {
                conn.job = null;
                job.conn = null;
                conn.idleSince = Date.now();
            } else {
                dropConn(conn);
            }
        }
    }

    function pump() {
        pumpTimer = null;
        try {
            assignJobs();
            for (const conn of conns.slice()) {
                if (conn.job)
                    service(conn);
            }
            assignJobs();
            const now = Date.now();
            for (const conn of conns.slice()) {
                if (!conn.job && now - conn.idleSince >= IDLE_CLOSE_MS)
                    dropConn(conn);
            }
        } catch (e) {
            console.error("guit3 WebUI bridge:", e);
        }

        if (queue.length || conns.some((c) => c.job))
            schedulePump(POLL_MS);
        else if (conns.length)
            schedulePump(IDLE_CLOSE_MS);
    }

    function schedulePump(ms) {
        if (pumpTimer !== null) {
            if (ms > 0)
                return;
            clearTimeout(pumpTimer);
        }
        pumpTimer = setTimeout(pump, ms);
    }

    /* ------------------------------------------------------------------ */
    /* Service Worker plumbing */

    function remember(list, value) {
        if (!value || list.includes(value))
            return;
        list.push(value);
        if (list.length > ROUTE_MAX)
            list.shift();
    }

    function routingState() {
        const launch = launchInfo();
        return {
            launch: launch ? [launch.path] : [],
            clients: servedClients.slice(),
            docs: servedDocs.slice(),
        };
    }

    function onWorkerMessage(e) {
        const msg = e.data;
        if (!msg)
            return;
        if (msg.type === "guit3-webui-adopt") {
            remember(servedClients, msg.clientId);
            remember(servedDocs, msg.doc);
        } else if (!e.ports.length) {
            return;
        } else if (msg.type === "guit3-webui-ping") {
            e.ports[0].postMessage({ type: "guit3-webui-pong", state: routingState() });
        } else if (msg.type === "guit3-webui-request") {
            acceptRequest(msg, e.ports[0]);
        }
    }

    /*
     *   Tell the worker this page is a bridge, and hand it our routing
     *   table - including the start page (if the game has launched its
     *   WebUI yet), which is how the WebUI page's own navigation gets
     *   routed to this VM in the first place. Resolves when the worker has
     *   acknowledged, or after a timeout.
     */
    function announce() {
        if (!worker)
            return Promise.resolve();
        return new Promise((resolve) => {
            const ch = new MessageChannel();
            const timer = setTimeout(() => { ch.port1.close(); resolve(); }, 2000);
            ch.port1.onmessage = () => { clearTimeout(timer); ch.port1.close(); resolve(); };
            worker.postMessage({ type: "guit3-webui-hello", state: routingState() }, [ch.port2]);
        });
    }

    /*
     *   Resolves once the worker routes the current WebUI start page to the
     *   VM. Whatever loads that page (step 6's overlay) should wait for this
     *   after the game launches - the "guit3-webui-launch" event and
     *   Module.onWebUILaunch() fire synchronously, before the worker has
     *   heard about the new path.
     */
    function whenRouted() {
        return routed;
    }

    /*
     *   Register the worker and announce this page as the bridge. Resolves
     *   once the worker is active and knows about us (or rejects if Service
     *   Workers are unavailable - they need a secure context, which
     *   http://localhost counts as but file:// doesn't).
     */
    async function start(swUrl) {
        if (!("serviceWorker" in navigator))
            throw new Error("Service Workers are unavailable here (insecure context?) - WebUI games can't run");

        navigator.serviceWorker.addEventListener("message", onWorkerMessage);
        const reg = await navigator.serviceWorker.register(swUrl || "guit3-webui-sw.js");
        if (new URL(reg.scope).pathname !== "/")
            console.warn("guit3 WebUI bridge: Service Worker scope is " + reg.scope
                + ", not the site root - WebUI requests (/webui/...) won't be intercepted."
                + " Serve guit3.html and guit3-webui-sw.js from the root.");
        const ready = await navigator.serviceWorker.ready;
        worker = ready.active;
        routed = announce();
        await routed;
        navigator.serviceWorker.addEventListener("controllerchange", () => {
            worker = navigator.serviceWorker.controller || worker;
            routed = announce();
        });
        setInterval(announce, ANNOUNCE_MS);
        return reg;
    }

    /* the game launched its WebUI (guit3_webui_launch_hook(), guit3.cpp) */
    window.addEventListener("guit3-webui-launch", () => {
        routed = announce();
    });

    function recent() {
        return recentLog.slice();
    }

    return { start, whenRouted, recent };
})();
