# WebUI over Emscripten via a Service-Worker loopback transport

## Problem

On Windows and Linux, a TADS Web UI game works by having `guit3` start an HTTP server thread and open the
game's start page in the user's real Web browser, which then talks to that server over a real TCP loopback
socket (`guit3_webui_launch_hook()` in [guit3.cpp](guit3.cpp), `osnet_connect_webui()` in
`tads-runner/tads3/unix/osnetunix.cpp`). Under Emscripten, `guit3` itself already runs *inside* a browser
tab, as WebAssembly with no OS sockets: there is no way for wasm code to `bind()`/`listen()`, so the
existing "start a server, hand the browser a URL" model cannot work as-is.

The good news is that the client and server no longer need a real network at all — both ends are in the
same browser. The goal is to replace the *transport* (real TCP loopback) with an in-browser one (some kind
of local message-passing between the wasm module and the WebUI page's normal `fetch`/`XHR`/`<img>`/etc.
traffic), while leaving `vmnet.cpp`, `vmhttpreq.cpp`, `lib/webui.t`, and every existing WebUI game's own
code completely unchanged. Ideally the fix lives entirely in the OS-glue layer plus a small amount of
Emscripten-side JS, not in the vendored VM/library code.

## Why a Service Worker

The WebUI client (`tads-runner/tads3/lib/webuires/*.js`) does not talk to the server through one API we
could monkey-patch. It uses, at minimum:

- long-polling `XMLHttpRequest`, created inside a helper iframe
  ([util.js:2546](../../../../tads-runner/tads3/lib/webuires/util.js#L2546))
- `<img src="...">` for on/off icons and cache-busting pings
  ([main.js:1535](../../../../tads-runner/tads3/lib/webuires/main.js#L1535))
- sub-window `<iframe>`s for file up/downloads
  ([main.js:3193](../../../../tads-runner/tads3/lib/webuires/main.js#L3193))
- `navigator.sendBeacon()` on tab close
  ([main.js:456](../../../../tads-runner/tads3/lib/webuires/main.js#L456))
- `window.open()` for the debug log window, plain HTML/CSS/audio resources, form uploads, download links —
  all under root-absolute URLs like `/webui/...`, `/webuires/...`

A [Service Worker](https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API)'s `fetch` event
sees *all* of these — every request the page and its same-origin frames make, regardless of which API
issued it — so it is the only interception point that covers the whole surface without touching game or
library code. This is the "catch and redirect browser events" approach, and it is the one that actually
generalizes.

## Where the VM already has a seam

The VM's HTTP handling doesn't know about sockets except at one narrow point:

- `TadsHttpServerThread` (`tads-runner/tads3/vmnet.h`) reads requests and writes HTTP/1.1 reply bytes
  straight to an `OS_Socket` via `send()`/`recv()` — see the reply path in
  [vmhttpreq.cpp:2454-2504](../../../../tads-runner/tads3/vmhttpreq.cpp#L2454-L2504).
- `OS_Listener`/`OS_Socket`/`OS_CoreSocket` (`tads-runner/tads3/unix/osnetunix.h:735-1130`) are the only
  places that call real BSD socket functions.

If those two small classes get an Emscripten-specific, in-memory "loopback" implementation, everything
above them — `vmnet.cpp`, `vmhttpsrv.cpp`, `vmhttpreq.cpp`, `webui.t`, and the game — needs no changes at
all.

## Proposed architecture

```
webui iframe (same origin)                         guit3 page (wasm)
  XHR / img / iframe / form / beacon                  JS bridge
          |  fetch event                                 |  ccall
          v                                               v
  Service Worker --postMessage(req)--> bridge --> loopback OS_Socket --> TadsHttpServerThread (pthread)
          ^                                                                    |  G_net_queue
          +---- Response (stream) <-- HTTP parser <-- EM_JS(outbound bytes) <-- VM main thread (sendReply)
```

The VM keeps thinking it's serving real HTTP over a real socket. The Service Worker keeps thinking it's
proxying real HTTP to a real server. Only the pipe between them is new.

## Blocker to fix first: blocking waits on the browser's main thread

`guit3` under Emscripten runs the VM on the browser's main thread (Asyncify, no `PROXY_TO_PTHREAD` for the
main executable). `getNetEvent()` waits via `pthread_cond_timedwait()`
([osnetunix.h:526](../../../../tads-runner/tads3/unix/osnetunix.h#L526)). On a real OS thread that blocks
and lets other threads run; on the browser's main thread under Emscripten it just spins without ever
yielding to the browser's event loop, so a Service Worker's `postMessage` could never be delivered and the
game would hang. Background listener/server threads (spawned via `pthread_create` from the main thread)
would have the same problem getting *started*.

`event_loop()` in `htmlgui.cpp` already had to solve exactly this class of problem for nested input waits
(see migration.md §5.10): the fix there was `emscripten_sleep()` (Asyncify's real suspend/resume
primitive), not a plain spin or `emscripten_set_main_loop()`. The networking waits need the same treatment:
under `__EMSCRIPTEN__`, when called from the main thread, `OS_Event::wait()`/`multi_wait()` need to loop on
the underlying signal with `emscripten_sleep()` yield points instead of `pthread_cond_wait`/`timedwait`.
Ideally that loop also keeps `guit3` rendering frames while it waits, the same way `event_loop()` does.

This has to be step 2, before any of the transport plumbing, or nothing downstream can be tested.

## Progress

- **Step 1 (JS-only Service Worker spike) — done, no blocker found.** See
  [emscripten/webui-sw-spike/README.md](emscripten/webui-sw-spike/README.md) for the full results.
- **Step 2 (main-thread blocking-wait fix) — implemented.** `OS_Event::evt_wait()`'s two overloads in
  `tads-runner/tads3/unix/osnetunix.h` now poll with `emscripten_sleep()` instead of blocking on
  `pthread_cond_wait()`/`pthread_cond_timedwait()`, but only when `emscripten_is_main_browser_thread()` is
  true - i.e. only for `guit3`'s own (non-`PROXY_TO_PTHREAD`) main thread, never for a real pthread Worker
  (a listener/server thread, or `t3run`/`t3core`'s proxied main), where a genuine blocking wait is correct
  and unchanged. `OS_Waitable::wait()`/`multi_wait()` (`osnetunix.cpp`) both bottom out through these same
  two overloads, so no other file needed to change. Verified with a full native (non-Emscripten) Linux
  build of `t3core` via WSL - `osnetunix.cpp` compiles clean and links, confirming the new code is
  syntactically sound and the untouched (non-`__EMSCRIPTEN__`) code path is unaffected. **Since verified for
  real**: installed emsdk (`C:\Projects\emsdk`, Windows build - the WSL-side `./emsdk` script doesn't run,
  it's CRLF-terminated for Windows use only) and built `guit3` with `cmake --preset emscripten` +
  `cmake --build build/emscripten --target guit3`; it compiles and links clean (`osnetunix.cpp.o` confirmed
  built into `t3htm`), and a headless-Chrome run of the resulting `guit3.html` (serving its packaged
  `tests/ditch3.t3`, with a throwaway local server adding the `Cross-Origin-Opener-Policy`/
  `Cross-Origin-Embedder-Policy` headers pthreads need) ran for ~25s with WebGL actually rendering (GPU
  readback messages in the console) and no uncaught exception, `abort()`, or `RuntimeError` - the page ended
  in the normal "fully loaded and running" state (`#status` empty, spinner hidden). A plain game like
  `ditch3.t3` never exercises `getNetEvent()`'s wait loop at all (nothing in it touches networking), so this
  confirms **no regression to ordinary startup**, not the new poll-wait branch actually firing - that still
  needs step 3+ (the loopback transport) before there's a real code path that calls it on Emscripten.

- **Step 3 (loopback transport) — done; live round-trip confirmed** (see "Step 3 stall: root causes and
  fixes" below, which supersedes the "not yet confirmed" paragraph at the end of this entry).
  New `tads-runner/tads3/emscripten/osnetloop.{h,cpp}` holds the whole transport: a listener table (with
  accept queues and auto-assigned ports from the dynamic range), a connection table with a growable byte
  queue per direction, one coarse global mutex, and a small `extern "C"` JS-facing API
  (`osu_loop_new_conn`/`push`/`pending_len`/`pull`/`end_conn`). `OS_Listener`/`OS_Socket` in
  `unix/osnetunix.h` delegate to it from `open()`/`accept()`/`send()`/`recv()`/`close()`/
  `set_non_blocking()`/`get_local_addr()`/`get_peer_addr()`, all behind `#ifdef __EMSCRIPTEN__` plus a
  runtime check, so every other platform and `t3run`/`t3core` are bit-for-bit unaffected. A loopback id
  rides in the existing inherited `int s` field, encoded as `-(id + 2)` so it can never collide with a real
  fd (always >= 0) or with `-1` ("not open") — this avoided adding any new member to those classes. Verified
  to compile and link clean both natively (WSL, `__EMSCRIPTEN__` off — the guarded code is a no-op there)
  and under the real Emscripten toolchain, with `osnetloop.cpp.o` confirmed built into `t3htm` and `guit3`
  relinking successfully.

  **Three real bugs/gaps this work surfaced, all fixed:**
  1. `osu_loop_available()` could not be a JS-driven runtime registration, as originally designed. A WebUI
     game opens its `HTTPServer` early enough in its own startup that no JS call can safely reach the wasm
     module first: Emscripten aborts any native call attempted before its runtime-init checkpoint
     (`native function ... called before runtime initialization`), and even a `Module.preRun` hook — which
     runs before `main()` and looked like the earliest safe place — still hit that assertion. It is now a
     compile-time default keyed on `T3_COMPILING_FOR_HTML` (this codebase's existing way to tell "compiled
     into t3htm, for guit3" from "compiled into t3core, for t3run"), so guit3 has the transport available
     from the first instruction and t3run keeps its real-socket path untouched.
  2. `guit3` needed `-s PTHREAD_POOL_SIZE` at all. Serving WebUI takes a listener thread plus one server
     thread per connection (`TadsListenerThread`/`TadsServerThread`), and on-demand Worker spawning from a
     non-main thread failed outright with Emscripten's `Tried to spawn a new thread, but the thread pool is
     exhausted.`
  3. `PTHREAD_POOL_SIZE=8` was too costly: the pool is pre-spawned (each worker instantiating its own copy
     of a ~7MB module) before the runtime counts as initialized, and the runtime never finished initializing
     within a headless test's patience. Reduced to 4.

  **What is not yet confirmed**: a full live HTTP request/response round-trip through a real game's
  `HTTPServer` over this transport. A throwaway harness for exactly that
  (`build/emscripten/.../step3-test.html`, driving `osu_loop_new_conn`/`push`/`pending_len`/`pull` by
  `ccall` against `tests/Webtest.t3`, no Service Worker involved) reliably gets as far as
  `onRuntimeInitialized` and then makes no further progress, with one renderer process burning >100% of a
  core. **Most of that investigation was invalidated by a mistake in the test orchestration rather than the
  code**: launching headless Chrome with a shell-level `&` meant the "task finished" signal actually
  reported the launcher script exiting immediately, not Chrome, so several "retries" piled additional full
  Chrome + worker-pool instances on top of ones still running and competing for CPU. A single clean isolated
  run still stalls, so there is something real left to find, but it is equally plausible that it is simply
  guit3's continuous software-rendered (swiftshader) frame loop plus a 4-worker pool being far slower than
  this test's patience, rather than a defect in the transport. **Next step before trusting step 3: re-run
  that harness in a non-headless browser (or with real GPU/more patience), where continuous rendering isn't
  software-rasterized, and find out whether the listener ever appears.** Do not assume the transport is
  broken on this evidence — and do not assume it works, either.

  **Step 3 stall: root causes and fixes.** It was not rendering speed. Temporary `fprintf(stderr)` tracing in
  `osnetloop.cpp` (plus relinking with `CMAKE_EXE_LINKER_FLAGS=--profiling-funcs` to get function names in
  wasm stack traces — remember to clear it again afterwards) showed three real bugs, found one behind the
  other:
  1. **The listener was closed again right after opening.** `guit3_webui_launch_hook()` called
     `os_open_url()`, which `fork()`s — always a failure in wasm — so the hook returned FALSE,
     `connectWebUI()` threw, and the game shut its `HTTPServer` down before JS could connect. The harness
     just saw "no listener on port 49152" forever. The hook now has an `__EMSCRIPTEN__` branch that opens
     nothing and publishes `Module.webuiLaunch = {port, path}` (and calls an optional
     `Module.onWebUILaunch(port, path)`), which is the natural hand-off point for step 6.
  2. **Server thread crashed (SAFE_HEAP alignment fault) on socket teardown.** Upstream latent bug:
     `OS_CoreSocket(int s)` never initialized `mon_thread` (only the default constructor does). Real
     sockets always overwrite it in `set_non_blocking()`, so it never showed; loopback sockets skip that, so
     `~OS_CoreSocket()` → `close()` dereferenced garbage. Fixed by initializing it in that constructor.
  3. **The whole reply was thrown away.** The VM writes the full reply, then (for `Connection: close`)
     closes the socket immediately, and `osu_loop_close()` freed the slot including the unread reply.
     Connections now half-close like TCP: `vm_closed`/`js_closed` flags, reply bytes stay pullable after a
     VM close, the slot is freed only when both ends have closed, and new `osu_loop_is_closed()` lets JS
     detect end-of-stream. **JS must call `osu_loop_end_conn()` exactly once per connection** or the slot
     leaks.

  Also fixed while in there: a lost-wakeup race. `OS_Listener::accept()`/`OS_Socket::recv()` reset
  `ready_evt` *after* `osu_loop_accept()`/`osu_loop_recv()` released the transport lock, so a JS
  `new_conn`/`push` landing in between would be lost and the listener/server thread would sleep forever. The
  reset now happens inside those functions under the lock, and `new_conn`/`push`/`end_conn` signal under the
  lock too (lock order: `g_loop_mutex`, then the event's own mutex). `guit3` also now exports
  `_malloc`/`_free`/`HEAPU8`, which JS needs to copy reply bytes out of `osu_loop_pull()`'s wasm-heap buffer
  (the harness failed with `Module._malloc is not a function` once it finally got that far).

  Result: the harness (headless Chrome, swiftshader, `tests/Webtest.t3`) gets `HTTP/1.1 200 OK` with the
  complete 7550-byte WebUI start page, `osu_loop_is_closed() == 1`, no abort, in well under 30s. The harness
  itself still only lives in `build/emscripten/.../step3-test.html` (unversioned); it now waits for
  `Module.webuiLaunch` instead of guessing port 49152, sends the real start-page path, and reads until
  close.

## Plan

1. **Spike (JS/browser only, no VM changes).** Register a Service Worker from a small standalone test page
   under `guit3`'s Emscripten output, serve a synthesized response to a same-origin iframe, and confirm:
   - which kinds of requests actually reach the Service Worker's `fetch` handler (XHR, `img`, `iframe`,
     `fetch`, forms) and which don't (`sendBeacon` is inconsistent across browsers and needs a fallback);
   - the COEP/COOP story: `guit3.html` is already cross-origin-isolated for pthreads
     (`Cross-Origin-Embedder-Policy: require-corp`), so every synthesized response needs matching
     `Cross-Origin-Resource-Policy`/CORP headers or the page's own isolation breaks;
   - Service Worker scope: the real WebUI uses root-absolute paths (`/webui/...`), so the worker must be
     registered at scope `/` (or the host needs to send `Service-Worker-Allowed: /`);
   - that a worker registered on first load doesn't control that first navigation (a Service Worker only
     starts controlling requests *after* it activates), which affects when COEP can first apply.
2. **Fix the main-thread blocking-wait problem** (above) in `osnetunix.h`, gated on `__EMSCRIPTEN__`, and
   confirm existing non-WebUI games are unaffected (the wait primitives are shared VM plumbing).
3. **Loopback transport**, a new `tads-runner/tads3/emscripten/osnetloop.{h,cpp}`:
   - `OS_Listener::accept()` waits on a "new connection" signal raised from JS instead of `::accept()`.
   - `OS_Socket::send()`/`recv()` move bytes through shared ring buffers instead of `::send()`/`::recv()`,
     signaling the existing `ready_evt`/`blocked_evt` events the rest of the code already expects.
   - A handful of `EM_JS`/`MAIN_THREAD_ASYNC_EM_ASM` entry points bridge to JS: `tads_loop_connect(port)`,
     `tads_loop_push(conn, bytes)`, `tads_loop_close(conn)`, plus a callback for outbound bytes (some sends
     happen from server threads, so outbound bytes need to hop back to the main thread).
   - Selected only when a JS bridge has actually registered itself, so `t3run`'s existing
     `ws://localhost:8888` bridge in
     [osnetemscipten.cpp](../../../../tads-runner/tads3/emscripten/osnetemscipten.cpp) is untouched.
4. **JS bridge**, added to `guit3.html`/a new Service Worker script, copied into the build output the same
   way `guit3.html` already is:
   - Serialize a `Request` into HTTP/1.1 bytes for the VM; parse the VM's raw reply bytes (status line,
     headers, `Content-Length` or chunked body) back into a streaming `Response` — streaming matters for
     the long-poll `getEvent` request and for file downloads.
   - Keep a small pool of keep-alive loopback connections (a handful, mirroring how many a real browser
     opens per origin) so the number of live server threads stays bounded.
   - New Emscripten link flags on `guit3` for the exported bridge functions and enough of a pthread pool
     for the extra server threads.
5. **Service Worker routing rule.** Route a request to the VM bridge if its `clientId` (or, for a
   navigation, its referrer) belongs to a registered WebUI frame; let everything else — including `guit3`'s
   own page assets — pass straight through unmodified. This also naturally covers the debug-log
   `window.open()` and file up/download sub-iframes, since they're same-origin children of a registered
   frame.
6. **Emscripten branch of the launch hook**, in `guit3_webui_launch_hook()` (`guit3.cpp`): drop the
   host/port entirely and instead point an overlay `<iframe>` (or a new tab — either works) at `path`.
   When that iframe/tab closes, have the host post a `TadsUICloseEvent` directly — a more reliable signal
   than the `sendBeacon`-based close detection the library normally relies on, and worth keeping even if
   `sendBeacon` also turns out to route through the worker.
7. **Validate** end-to-end against `tads-runner/tests/Webtest.t3` and at least one third-party WebUI game:
   - file upload (multipart POST must pass through as opaque bytes) and download
     (`Content-Disposition`, per `lib/webui.t`'s download table around line 601);
   - the file-picker fallback: `osnet_askfile()` already returns failure everywhere off Windows, so
     `webui.t`'s `getInputFile()` already falls back to a plain browser upload/download dialog — that path
     should just work unchanged, though persisted saves land in browser storage rather than a real
     filesystem, which is a separate, later question.

## Alternatives considered and set aside

- **Patch `lib/webuires/*.js` at game-load time.** Only catches `XMLHttpRequest`, and every WebUI game
  ships its own copy of these files (they're library resources bundled with the game, not shared at
  runtime), so a "smart" central patch can't reach them without a build-time or load-time rewrite step of
  its own. Worth keeping in reserve only as a fallback for contexts where Service Workers are unavailable
  (`file://` URLs, Firefox private browsing) — not as the primary mechanism.
- **Emscripten's own POSIX-socket-over-WebSocket emulation.** Provides a client-style `connect()`, but no
  `listen()`/`accept()` in the browser — there's nothing on the other end to listen. This is what
  `osnetemscipten.cpp`'s existing `ws://localhost:8888` bridge already works around for `t3run` by dialing
  out to an external process; it doesn't help `guit3`, which has no external process to dial.
- **Skip sockets entirely — hand `TadsHttpRequest` objects straight to `G_net_queue` from JS.** Would avoid
  needing loopback `OS_Socket`s at all, but the *reply* path (`TadsHttpServerThread::send()`) is still
  socket-shaped in the vendored VM code, so this only moves the seam rather than avoiding it, while being a
  bigger departure from the existing `TadsListener`/`TadsServerThread` design. Worth revisiting only if a
  later goal is dropping pthreads from the Emscripten build entirely.

## Side benefits

- `guit3` under Emscripten would no longer depend on the external `ws://localhost:8888` bridge process at
  all for WebUI games specifically (non-WebUI games still use it for other networking features, if any).
- The same Service Worker can add the `Cross-Origin-Opener-Policy`/`Cross-Origin-Embedder-Policy` headers
  `guit3.html` itself needs for pthreads, which static hosts (e.g. GitHub Pages) can't set via plain HTTP
  headers — potentially letting `guit3` run from a static host without a custom server at all.

## Risk summary

The two open questions that gate everything else are the main-thread blocking-wait fix (step 2 — this is
shared VM plumbing, so a mistake there risks regressing non-WebUI Emscripten games) and the Service Worker
routing/COEP details (step 1). That's why the plan starts with a JS-only spike before touching any VM code.
