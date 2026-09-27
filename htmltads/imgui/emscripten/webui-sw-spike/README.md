# Service Worker feasibility spike (webui-emscripten-plan.md step 1)

Standalone, VM-free test harness. It answers the four questions in
[../../webui-emscripten-plan.md](../../webui-emscripten-plan.md)'s step 1 before any real VM/loopback code
gets written:

1. Does a Service Worker's `fetch` handler actually see XHR, `<img>`, same-origin `<iframe>` navigations,
   `fetch()`, and `sendBeacon()` requests?
2. Do synthesized (in-worker) responses survive a page that's cross-origin-isolated
   (`Cross-Origin-Embedder-Policy: require-corp`), or do they need explicit
   `Cross-Origin-Resource-Policy` headers to avoid being blocked?
3. Does registering the worker at scope `/webui-sw-spike/` let it intercept a request made by a same-origin
   child iframe under that scope (mirroring `util.js`'s "create the XHR inside a helper iframe" trick)?
4. Does the very first page load (before the worker has activated) go through the worker, or only later
   loads/reloads?

## Files

- `server.py` - a tiny static file server (stdlib only) that adds `Cross-Origin-Opener-Policy: same-origin`
  and `Cross-Origin-Embedder-Policy: require-corp` to every response, so the page is cross-origin-isolated
  exactly like the real `guit3.html` is. Plain `python -m http.server` can't set custom headers, hence this.
- `index.html` - the test harness. Registers `service-worker.js`, then runs each check in turn and writes a
  machine-readable summary into `#results` (and mirrors every step to `console.log` with a `SPIKE:` prefix,
  for headless capture).
- `service-worker.js` - intercepts any request under `/webui-sw-spike/api/` and synthesizes a response,
  standing in for "the VM answered this over the loopback transport"; everything else passes through
  untouched, exactly like the real routing rule in the plan would.
- `child.html` - loaded in a same-origin iframe by `index.html`, to exercise question 3.

## Running it manually

```
python server.py
```

Then open `http://localhost:8791/webui-sw-spike/` in Chrome or Edge (needs a real HTTP origin, not
`file://` - Service Workers require a secure context, and `http://localhost` counts as one). Reload once
after the first load (see question 4) and check the `#results` panel and the DevTools Console/Network/
Application > Service Workers tabs.

## Findings

Ran headless (`chrome.exe --headless=new --dump-dom`, with the harness writing its results into the
`#results` `<pre>` for a headless run to scrape) against a fresh profile, then again with the same profile
after the worker had already registered and activated once. All four questions came back positive, with no
real blocker found:

1. **Every kind of request is seen.** `fetch()`, `XMLHttpRequest`, `<img src>`, and an `XMLHttpRequest`
   issued from inside a same-origin child `<iframe>` were all intercepted and answered by the worker
   (`fromWorker: true` in each case). `navigator.sendBeacon()` was also intercepted in Chrome - the plan
   doc's caution that beacon routing might be inconsistent still holds for *other* browsers/older versions,
   but it is not a Chrome problem.
2. **Synthesized responses survive a COEP-isolated page.** `crossOriginIsolated` was `true` throughout, and
   the worker's synthesized `fetch`/XHR/iframe responses were consumed without being blocked - **as long as
   they carry `Cross-Origin-Resource-Policy`**. This header is required on every synthesized response for a
   page in this configuration; the plan already assumed this, and the spike confirms it's necessary rather
   than just cautious.
3. **A same-origin child iframe under the worker's scope is covered.** `child.html`'s `XMLHttpRequest`
   (mirroring `util.js`'s "create the XHR inside a helper iframe" pattern) was intercepted exactly like a
   request from the top-level page - no separate registration or scope needed for the child frame.
4. **The first load is not controlled, but doesn't need a reload either.** `controllerOnFirstLoad` was
   `false` on a fresh profile (expected - a Service Worker never controls the navigation that first
   registers it), but calling `clients.claim()` from the worker's `activate` handler adopted the *already
   open* tab within ~250ms, without any reload: `controllerAfterActivate` came back `true` in the same
   session. A second, later navigation (simulating the user reloading, or a fresh tab after the worker is
   already installed) was controlled from the very first byte (`controllerOnFirstLoad: true`). **This means
   the real plan doesn't need a reload trick or a "please wait, installing..." step**: register the worker,
   await `navigator.serviceWorker.ready`, call `clients.claim()` in `activate`, and the current tab can
   start being intercepted almost immediately.

One real bug found and fixed by the run itself, not just a theoretical concern: the `beacon` handler
originally returned `new Response("", { status: 204, ... })`, which throws (`Response with null body status
cannot have body`) - a 204/205/304 response must use `null`, not `""`, as its body. Harmless for a
fire-and-forget beacon (the client never reads the reply), but worth remembering for the real
implementation's own 204/304 replies.

**Conclusion: no blocker found in the browser-side mechanics.** The remaining open risk in
`webui-emscripten-plan.md` is entirely on the VM side - the main-thread blocking-wait fix (plan step 2) -
not in the Service Worker approach itself.
