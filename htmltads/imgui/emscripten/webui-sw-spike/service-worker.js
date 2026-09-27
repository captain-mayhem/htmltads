/*
 *   Service Worker feasibility spike - see ../../webui-emscripten-plan.md step 1 and
 *   this folder's README.md.
 *
 *   Stands in for the real plan's routing rule: anything under /webui-sw-spike/api/ is
 *   answered here directly, as a stand-in for "the request reached the VM's loopback
 *   transport and got a reply back"; everything else is passed straight through to the
 *   network, exactly as the real worker would for guit3's own page assets.
 */

const API_PREFIX = "/webui-sw-spike/api/";

self.addEventListener("install", (event) => {
    // Activate immediately rather than waiting for all tabs to close - the real
    // deployment wants an update to take effect on the next load, not the next
    // full browser restart.
    self.skipWaiting();
});

self.addEventListener("activate", (event) => {
    // clients.claim() lets an *already open* tab be adopted by this worker without a
    // reload - relevant to question 4 in README.md (does the first load go through the
    // worker at all?).
    event.waitUntil(self.clients.claim());
});

self.addEventListener("fetch", (event) => {
    const url = new URL(event.request.url);

    if (!url.pathname.startsWith(API_PREFIX)) {
        // Not one of "our" simulated VM endpoints - pass through untouched.
        return;
    }

    event.respondWith(handleApiRequest(event.request, url));
});

async function handleApiRequest(request, url) {
    const name = url.pathname.slice(API_PREFIX.length);
    const commonHeaders = {
        // Required for a synthesized response to survive a COEP:require-corp page -
        // this is exactly what question 2 in README.md is checking. Comment this out
        // to see the isolated page reject the response instead.
        "Cross-Origin-Resource-Policy": "same-origin",
    };

    if (name === "echo") {
        const body = await request.text().catch(() => "");
        return new Response(
            JSON.stringify({
                ok: true,
                method: request.method,
                url: request.url,
                bodyLen: body.length,
                fromWorker: true,
            }),
            {
                status: 200,
                headers: { ...commonHeaders, "Content-Type": "application/json" },
            }
        );
    }

    if (name === "pixel.gif") {
        // A real 1x1 transparent GIF, base64-decoded - exercises the <img src> path
        // (question 1), which can't carry a JSON body.
        const b64 =
            "R0lGODlhAQABAIAAAAAAAP///yH+EUNyZWF0ZWQgd2l0aCBHSU1QACH5BAEKAAAALAAAAAABAAEAAAICTAEAOw==";
        const bytes = Uint8Array.from(atob(b64), (c) => c.charCodeAt(0));
        return new Response(bytes, {
            status: 200,
            headers: { ...commonHeaders, "Content-Type": "image/gif" },
        });
    }

    if (name === "beacon") {
        // navigator.sendBeacon() targets - the client never reads the response, but we
        // record that we got it (see /webui-sw-spike/api/beacon-log below) so the test
        // harness can find out whether the beacon reached the worker at all.
        const body = await request.text().catch(() => "");
        beaconLog.push({ t: Date.now(), bodyLen: body.length });
        // A 204 response must have a null body - an empty string still counts as "has a
        // body" and throws. (Found by the spike run itself: Chrome logged the resulting
        // FetchEvent as a network error even though the client never looks at it, since
        // sendBeacon() is fire-and-forget - see README.md findings.)
        return new Response(null, { status: 204, headers: commonHeaders });
    }

    if (name === "beacon-log") {
        return new Response(JSON.stringify(beaconLog), {
            status: 200,
            headers: { ...commonHeaders, "Content-Type": "application/json" },
        });
    }

    return new Response("not found", { status: 404, headers: commonHeaders });
}

// In-memory only - fine for a spike; a worker can be killed and restarted by the
// browser between events, so this isn't a reliable store, only good enough to let the
// harness poll "did my beacon arrive" shortly after sending it in the same session.
const beaconLog = [];
