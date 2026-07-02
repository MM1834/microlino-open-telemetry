const CACHE = "mot-dashboard-v0.1";
const ASSETS = [
  "./",
  "./index.html",
  "./config.js",
  "./css/style.css",
  "./js/app.js",
  "./js/mqtt.min.js",
  "./manifest.json"
];

self.addEventListener("install", (event) => {
  event.waitUntil(caches.open(CACHE).then((cache) => cache.addAll(ASSETS)));
});

self.addEventListener("fetch", (event) => {
  event.respondWith(caches.match(event.request).then((cached) => cached || fetch(event.request)));
});
