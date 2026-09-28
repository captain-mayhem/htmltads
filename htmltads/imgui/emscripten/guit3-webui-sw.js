/*
 *   guit3 WebUI Service Worker - the browser half of the WebUI-over-Emscripten
 *   transport (webui-emscripten-plan.md, step 4).
 *
 *   A WebUI game's client pages talk to the game's HTTPServer with ordinary
 *   same-origin XHR/<img>/<iframe>/form/beacon traffic. Under Emscripten there
 *   is no real server socket for that traffic to reach, so this worker catches
 *   it in its fetch handler and relays each request, as plain data, to the
 *   guit3 page that hosts the VM. That page's bridge (guit3-webui-bridge.js)
 *   turns it into HTTP/1.1 bytes on a loopback connection
 *   (tads3/emscripten/osnetloop.h), parses the VM's reply, and streams it back
 *   here as a "head" message followed by "chunk" messages and an "end" - which
 *   this worker turns into a streaming Response.
 *
 *   Deliberately thin: all HTTP knowledge (serialization, parsing, cookies,
 *   the connection pool) lives in the bridge, next to the VM whose lifetime it
 *   shares. A Service Worker can be stopped and restarted by the browser
 *   between events and loses every global when that happens, so nothing here
 *   is state that has to survive.
 *
 *   Must be served from (and so scoped to) the site root: WebUI pages use
 *   root-absolute URLs (/webui/..., /webuires/...), which only a worker whose
 *   scope is "/" can see.
 */

"use strict";

/*
 *   Which requests go to the VM. PROVISIONAL (step 4): the fixed WebUI
 *   library prefixes, plus the exact path of the game's start page, which
 *   the bridge announces once the game launches its WebUI session (a game
 *   can put that anywhere - tests/Webtest.t3 serves it at "/"). Step 5 of
 *   the plan replaces this with a per-client rule (anything requested by a
 *   registered WebUI frame), which also covers game-defined resource paths
 *   outside these prefixes.
 */
const VM_PREFIXES = ["/webui/", "/webuires/"];

/*
 *   Headers every synthesized response needs. guit3.html is
 *   cross-origin-isolated (COOP same-origin + COEP require-corp) for
 *   pthreads: a subresource without CORP is blocked (the step 1 spike
 *   confirmed this); a nested document - the WebUI page's own subwindow
 *   iframes - without its own COEP is refused inside an isolated page; and
 *   the WebUI page itself, opened as a top-level tab from guit3.html, must
 *   carry the same COOP or the browser severs it from its opener (guit3
 *   then loses its handle on the tab, e.g. to notice it closing).
 */
const ISOLATION_HEADERS = [
    ["Cross-Origin-Resource-Policy", "same-origin"],
    ["Cross-Origin-Embedder-Policy", "require-corp"],
    ["Cross-Origin-Opener-Policy", "same-origin"],
];

/*
 *   Injected at the top of every WebUI HTML document this worker serves.
 *
 *   Why: a request only reaches this worker if the frame issuing it is
 *   controlled by it, and Chrome does not extend control to a same-origin
 *   child frame whose document was built in script (about:blank, then
 *   document.open()/write()) - found by the step 4 end-to-end test, not the
 *   step 1 spike, which only tried a child frame loaded from a real URL.
 *   The WebUI library creates all of its XMLHttpRequests inside exactly
 *   such a frame (webuires/util.js, initXmlFrame(): an old Safari
 *   progress-throbber workaround), so every getState/getEvent/... request
 *   went straight to the network and 404'd.
 *
 *   Fix, without touching the game's own copy of the library: when the
 *   page asks an iframe for its contentWindow, give a script-built frame
 *   (about:blank, or document.open()ed so its URL is the parent's) an
 *   XMLHttpRequest that constructs the parent's - controlled - kind of
 *   request instead. A frame navigated to a real URL is controlled itself
 *   and keeps its own; the check runs per request, since a frame's initial
 *   about:blank Window object can be reused for its first real document.
 */
const FRAME_XHR_SHIM = `<script>/* guit3 WebUI: see guit3-webui-sw.js */(function(){
var P=window.XMLHttpRequest,d=Object.getOwnPropertyDescriptor(HTMLIFrameElement.prototype,"contentWindow");
if(!d||!d.get)return;
Object.defineProperty(HTMLIFrameElement.prototype,"contentWindow",{configurable:true,enumerable:d.enumerable,get:function(){
var w=d.get.call(this);
try{if(w&&!w.__guit3XhrShim){var F=w.XMLHttpRequest;w.__guit3XhrShim=true;
w.XMLHttpRequest=function(){var u=w.location.href;return(u==="about:blank"||u===window.location.href)?new P():new F();};}}catch(e){}
return w;}});})();</script>`;

/* statuses whose Response must have a null body (the constructor throws otherwise) */
const NULL_BODY_STATUS = new Set([101, 103, 204, 205, 304]);

/* the guit3 page's client id, as announced by its bridge's hello message */
let bridgeClientId = null;

/*
 *   Start page pathnames (no query) the bridge has announced. Lost if the
 *   browser restarts this worker, so the bridge re-announces periodically.
 */
let launchPaths = new Set();

self.addEventListener("install", () => {
    self.skipWaiting();
});

self.addEventListener("activate", (event) => {
    /* adopt the already-open guit3 tab without a reload (spike finding 4) */
    event.waitUntil(self.clients.claim());
});

self.addEventListener("message", (event) => {
    const msg = event.data;
    if (msg && msg.type === "guit3-webui-hello" && event.source) {
        bridgeClientId = event.source.id;
        launchPaths = new Set(msg.paths || []);
        if (event.ports.length)
            event.ports[0].postMessage({ type: "guit3-webui-hello-ack" });
    }
});

function isVMRequest(url) {
    return url.origin === self.location.origin
        && (VM_PREFIXES.some((p) => url.pathname.startsWith(p))
            || launchPaths.has(url.pathname));
}

self.addEventListener("fetch", (event) => {
    const req = event.request;
    const url = new URL(req.url);
    if (!isVMRequest(url))
        return;     /* not ours - let the network handle it untouched */

    let done;
    const finished = new Promise((resolve) => { done = resolve; });
    /* keep the worker alive until the body has been fully relayed, not just the head */
    event.waitUntil(finished);
    const reply = relayToVM(req, url, done);
    event.respondWith((req.mode === "navigate" ? reply.then(injectShim) : reply).catch((e) => {
        done();
        return errorResponse(502, "guit3 WebUI bridge error: " + e);
    }));
});

/*
 *   Find the bridge page. After a worker restart bridgeClientId is gone, so
 *   fall back to asking every window client; only a page running the bridge
 *   answers.
 */
async function findBridge() {
    if (bridgeClientId) {
        const c = await self.clients.get(bridgeClientId);
        if (c)
            return c;
        bridgeClientId = null;
    }
    const all = await self.clients.matchAll({ type: "window", includeUncontrolled: true });
    for (const c of all) {
        if (await pingBridge(c)) {
            bridgeClientId = c.id;
            return c;
        }
    }
    return null;
}

function pingBridge(client) {
    return new Promise((resolve) => {
        const ch = new MessageChannel();
        const timer = setTimeout(() => { ch.port1.close(); resolve(false); }, 500);
        ch.port1.onmessage = () => { clearTimeout(timer); ch.port1.close(); resolve(true); };
        client.postMessage({ type: "guit3-webui-ping" }, [ch.port2]);
    });
}

/*
 *   Insert FRAME_XHR_SHIM into an HTML document reply - right after <head>
 *   if there is one, else after the doctype (never before it, which would
 *   drop the page into quirks mode). Works on raw bytes so the document's
 *   own encoding passes through untouched. Buffers the whole document,
 *   which is fine for a page; everything that needs streaming (the event
 *   long poll, downloads) is not a navigation.
 */
async function injectShim(resp) {
    const type = resp.headers.get("Content-Type") || "";
    if (!resp.body || !/^text\/html/i.test(type))
        return resp;
    const bytes = new Uint8Array(await resp.arrayBuffer());
    const scan = String.fromCharCode.apply(null, bytes.subarray(0, 4096)).toLowerCase();
    let at = 0;
    const head = /<head[\s>]/.exec(scan);
    const doctype = /<!doctype[^>]*>/.exec(scan);
    if (head)
        at = scan.indexOf(">", head.index) + 1;
    else if (doctype)
        at = doctype.index + doctype[0].length;
    const shim = new TextEncoder().encode(FRAME_XHR_SHIM);
    const out = new Uint8Array(bytes.length + shim.length);
    out.set(bytes.subarray(0, at), 0);
    out.set(shim, at);
    out.set(bytes.subarray(at), at + shim.length);
    const headers = new Headers(resp.headers);
    headers.delete("Content-Length");
    return new Response(out, { status: resp.status, statusText: resp.statusText, headers });
}

function errorResponse(status, text) {
    return new Response(text, {
        status,
        headers: [...ISOLATION_HEADERS, ["Content-Type", "text/plain; charset=utf-8"]],
    });
}

async function relayToVM(req, url, done) {
    const bridge = await findBridge();
    if (!bridge) {
        done();
        return errorResponse(503, "guit3 WebUI bridge is not running");
    }

    /* the body is relayed as opaque bytes - multipart uploads included */
    const body = (req.method === "GET" || req.method === "HEAD")
        ? null : await req.arrayBuffer();

    const ch = new MessageChannel();
    const port = ch.port1;
    const cancel = () => {
        try { port.postMessage({ type: "cancel" }); } catch (e) { }
        port.close();
        done();
    };
    if (req.signal)
        req.signal.addEventListener("abort", cancel);

    bridge.postMessage({
        type: "guit3-webui-request",
        method: req.method,
        target: url.pathname + url.search,
        headers: [...req.headers],
        body,
    }, body ? [ch.port2, body] : [ch.port2]);

    return new Promise((resolve) => {
        let controller = null;
        port.onmessage = (e) => {
            const m = e.data;
            switch (m.type) {
            case "head": {
                const headers = new Headers(m.headers);
                for (const [k, v] of ISOLATION_HEADERS)
                    headers.set(k, v);
                const noBody = req.method === "HEAD" || NULL_BODY_STATUS.has(m.status);
                const stream = noBody ? null : new ReadableStream({
                    start(c) { controller = c; },
                    cancel,
                });
                try {
                    resolve(new Response(stream, {
                        status: m.status,
                        statusText: m.statusText,
                        headers,
                    }));
                } catch (err) {
                    /* e.g. a status outside the 200-599 a Response may carry */
                    resolve(errorResponse(502, "unusable reply from the game: " + err));
                    cancel();
                }
                break;
            }
            case "chunk":
                try {
                    if (controller)
                        controller.enqueue(new Uint8Array(m.bytes));
                } catch (err) {
                    /* the consumer already cancelled the stream */
                }
                break;
            case "end":
                if (controller)
                    controller.close();
                port.close();
                done();
                break;
            case "error":
                if (controller)
                    controller.error(new Error(m.message));
                else
                    resolve(errorResponse(502, m.message));
                port.close();
                done();
                break;
            }
        };
    });
}
