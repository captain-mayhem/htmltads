/*
 *   guit3 WebUI Service Worker - the browser half of the WebUI-over-Emscripten
 *   transport (webui-emscripten-plan.md, steps 4 and 5).
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
 *   shares. The one thing this worker does own is the routing table (see
 *   "Routing" below), and even that is only a cache of what the bridges
 *   hold: a Service Worker can be stopped and restarted by the browser
 *   between events and loses every global when that happens.
 *
 *   Must be served from (and so scoped to) the site root: WebUI pages use
 *   root-absolute URLs (/webui/..., /webuires/...), which only a worker whose
 *   scope is "/" can see.
 */

"use strict";

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

/*
 *   Also injected, ahead of FRAME_XHR_SHIM, into the WebUI start page when
 *   guit3 hosts it in its overlay <iframe> (plan step 6) rather than a tab.
 *
 *   The WebUI library assumes its main page is the top-level window: every
 *   walk of its window tree (util.js's $win(), utilInit()'s
 *   window.parent.pathFromWindow(), setDefaultFocus(), popupCloser(),
 *   debugLog()) climbs window.parent until it reaches a window that is its
 *   own parent, and expects that to be the main page. Framed by guit3.html
 *   it would climb right out into guit3's page and fail (step 4 finding 6).
 *   Window.parent is a [Replaceable] attribute, so the main page can simply
 *   become its own parent: every walk - including those from its subwindow
 *   frames, which read the main page's parent property - then stops where
 *   the library expects. Window.top is left alone; the library never uses
 *   it.
 */
const HOSTED_START_SHIM = "<script>/* guit3 WebUI: see guit3-webui-sw.js */window.parent=window;</script>";

/* statuses whose Response must have a null body (the constructor throws otherwise) */
const NULL_BODY_STATUS = new Set([101, 103, 204, 205, 304]);

/*
 *   Routing (plan step 5). Which requests belong to a VM is decided by who
 *   makes them, not by their path - a game serves whatever paths it likes
 *   (Webtest.t3's start page is "/", its subwindows "/cmdwin.htm", ...):
 *
 *   - a navigation to a start page a bridge announced (the target, query
 *     included - it carries the session key, so it's unique per VM) goes to
 *     that bridge's VM. That's the WebUI page itself, opened by guit3;
 *   - a navigation whose referrer is a document a VM served goes to that VM.
 *     That covers the WebUI page's subwindow iframes, its file up/download
 *     frames, and the debug log's window.open();
 *   - any other request from a client (frame/tab) whose document a VM served
 *     goes to that VM: XHRs, images, scripts, forms, beacons.
 *
 *   Everything else - guit3.html's own page and assets above all - is left
 *   alone and goes to the network untouched, even at a path like /webui/.
 *   Keying everything by bridge also keeps two guit3 tabs, each running its
 *   own game, from ever seeing each other's traffic.
 *
 *   bridges: bridge (guit3 page) client id -> {
 *       launch:  Set of start page targets (pathname + search),
 *       clients: Set of client ids of documents this VM served,
 *       docs:    Set of URLs (no fragment) of documents this VM served,
 *   }
 *
 *   A document is recorded when its navigation is routed ("adopted"), and
 *   the bridge is told too, since it outlives this worker: every bridge
 *   re-announces its whole table periodically, and answers the ping below
 *   with it. After a worker (re)start, 'restored' is false and the first
 *   fetch waits for restoreState() to collect every bridge's table before
 *   deciding anything - otherwise the WebUI page's next request after a
 *   restart would fall through to the network and 404.
 */
const bridges = new Map();
let restored = false;
let restoring = null;

function applyState(bridgeId, st) {
    bridges.set(bridgeId, {
        launch: new Set(st.launch || []),
        clients: new Set(st.clients || []),
        docs: new Set(st.docs || []),
    });
}

/* the bridge id a request belongs to, or null if it isn't a VM's */
function routeFor(req, url, clientId) {
    if (url.origin !== self.location.origin)
        return null;
    if (clientId) {
        for (const [id, b] of bridges) {
            if (b.clients.has(clientId))
                return id;
        }
    }
    if (req.mode === "navigate") {
        const target = url.pathname + url.search;
        for (const [id, b] of bridges) {
            if ((req.referrer && b.docs.has(req.referrer)) || b.launch.has(target))
                return id;
        }
    }
    return null;
}

/* record a document a VM is about to serve, here and with its bridge */
function adopt(bridgeId, clientId, url) {
    const b = bridges.get(bridgeId);
    if (!b)
        return;
    const doc = url.href.split("#")[0];
    if (clientId)
        b.clients.add(clientId);
    b.docs.add(doc);
    self.clients.get(bridgeId).then((c) => {
        if (c)
            c.postMessage({ type: "guit3-webui-adopt", clientId, doc });
    });
}

/* rebuild the routing table from the bridges, once per worker lifetime */
function restoreState() {
    if (!restoring) {
        restoring = (async () => {
            const all = await self.clients.matchAll({ type: "window", includeUncontrolled: true });
            await Promise.all(all.map(async (c) => {
                const st = await pingBridge(c);
                if (st && !bridges.has(c.id))
                    applyState(c.id, st);
            }));
            restored = true;
        })();
    }
    return restoring;
}

/* ask a window client for its bridge state; resolves null if it isn't a bridge */
function pingBridge(client) {
    return new Promise((resolve) => {
        const ch = new MessageChannel();
        const timer = setTimeout(() => { ch.port1.close(); resolve(null); }, 300);
        ch.port1.onmessage = (e) => {
            clearTimeout(timer);
            ch.port1.close();
            resolve((e.data && e.data.state) || null);
        };
        client.postMessage({ type: "guit3-webui-ping" }, [ch.port2]);
    });
}

self.addEventListener("install", () => {
    self.skipWaiting();
});

self.addEventListener("activate", (event) => {
    /* adopt the already-open guit3 tab without a reload (spike finding 4) */
    event.waitUntil(self.clients.claim());
});

self.addEventListener("message", (event) => {
    const msg = event.data;
    if (!msg || !event.source)
        return;
    if (msg.type === "guit3-webui-hello") {
        /* a bridge's full table - replaces whatever we had cached for it */
        applyState(event.source.id, msg.state || {});
    } else if (msg.type === "guit3-webui-forget") {
        /* testing aid: drop the routing table exactly as a worker restart would */
        bridges.clear();
        restored = false;
        restoring = null;
    } else {
        return;
    }
    if (event.ports.length)
        event.ports[0].postMessage({ type: msg.type + "-ack" });
});

self.addEventListener("fetch", (event) => {
    const req = event.request;
    const url = new URL(req.url);
    if (url.origin !== self.location.origin)
        return;

    if (restored) {
        const bridgeId = routeFor(req, url, event.clientId);
        if (bridgeId !== null)
            serve(event, bridgeId, url);
        /* else not ours - let the network handle it untouched */
        return;
    }

    /*
     *   First fetch since this worker (re)started: the routing table has to
     *   be rebuilt before this request can be classified, so answer it
     *   asynchronously - relayed if it turns out to be a VM's, otherwise
     *   fetched from the network on the page's behalf.
     */
    let done;
    event.waitUntil(new Promise((resolve) => { done = resolve; }));
    event.respondWith(restoreState().then(() => {
        const bridgeId = routeFor(req, url, event.clientId);
        if (bridgeId === null) {
            done();
            return fetch(req);
        }
        return reply(req, url, bridgeId, event.resultingClientId, done);
    }));
});

function serve(event, bridgeId, url) {
    let done;
    /* keep the worker alive until the body has been fully relayed, not just the head */
    event.waitUntil(new Promise((resolve) => { done = resolve; }));
    event.respondWith(reply(event.request, url, bridgeId, event.resultingClientId, done));
}

/* the VM's Response to a routed request; never rejects */
function reply(req, url, bridgeId, resultingClientId, done) {
    let r;
    if (req.mode === "navigate") {
        const b = bridges.get(bridgeId);
        const hostedStart = req.destination === "iframe"
            && b.launch.has(url.pathname + url.search)
            && !b.docs.has(req.referrer);
        adopt(bridgeId, resultingClientId, url);
        const shim = (hostedStart ? HOSTED_START_SHIM : "") + FRAME_XHR_SHIM;
        r = relayToVM(req, url, bridgeId, done).then((resp) => injectShim(resp, shim));
    } else {
        r = relayToVM(req, url, bridgeId, done);
    }
    return r.catch((e) => {
        done();
        return errorResponse(502, "guit3 WebUI bridge error: " + e);
    });
}

/*
 *   Insert the shim script(s) into an HTML document reply - right after <head>
 *   if there is one, else after the doctype (never before it, which would
 *   drop the page into quirks mode). Works on raw bytes so the document's
 *   own encoding passes through untouched. Buffers the whole document,
 *   which is fine for a page; everything that needs streaming (the event
 *   long poll, downloads) is not a navigation.
 */
async function injectShim(resp, shimText) {
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
    const shim = new TextEncoder().encode(shimText);
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

async function relayToVM(req, url, bridgeId, done) {
    const bridge = await self.clients.get(bridgeId);
    if (!bridge) {
        /* the guit3 tab running this game is gone, and with it the game */
        bridges.delete(bridgeId);
        done();
        return errorResponse(503, "the game serving this page is no longer running");
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
