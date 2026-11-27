/* Cross-origin isolation for hosts that cannot send headers (GitHub Pages): the page registers this file as a service worker, which adds
 * Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy to every response, and reloads once so that the page is isolated.
 * The game runs on a thread and needs SharedArrayBuffer. */
if (typeof window === "undefined") {
    self.addEventListener("install", () => self.skipWaiting());
    self.addEventListener("activate", (event) => event.waitUntil(self.clients.claim()));
    self.addEventListener("fetch", (event) => {
        const request = event.request;

        if (request.cache === "only-if-cached" && request.mode !== "same-origin")
            return;
        event.respondWith(fetch(request).then((response) => {
            if (response.status === 0)
                return response;
            const headers = new Headers(response.headers);

            headers.set("Cross-Origin-Embedder-Policy", "require-corp");
            headers.set("Cross-Origin-Opener-Policy", "same-origin");
            return new Response(response.body, { status: response.status, statusText: response.statusText, headers });
        }));
    });
} else if (!window.crossOriginIsolated && window.isSecureContext && "serviceWorker" in navigator) {
    let reloaded = false;

    try {
        reloaded = sessionStorage.getItem("coi-reloaded") === "1";
        sessionStorage.setItem("coi-reloaded", "1");
    } catch (e) {}
    navigator.serviceWorker.register(document.currentScript.src).then((registration) => {
        if (!reloaded && (registration.active || registration.installing || registration.waiting))
            navigator.serviceWorker.ready.then(() => window.location.reload());
    });
}
