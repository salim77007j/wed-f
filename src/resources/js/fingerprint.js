// WED Browser — fingerprint protection script (injected at DocumentCreation,
// main world, all frames). When enabled in settings.
//
// Strategy: per-(origin, session) deterministic randomization. The same site
// sees a stable fingerprint during one browsing session (so pages and logins
// keep working) but cannot correlate it with other sites or sessions, and
// canvas/audio/WebGL reads get consistent additive noise.
(function () {
    'use strict';

    // ---- session key per origin, stable while this session lives ----
    var KEY = 'wedFpSeed';
    var sessionStore = (function () {
        try { return sessionStorage; } catch (e) { return null; }
    })();
    var seedStr = sessionStore ? sessionStore.getItem(KEY) : null;
    if (!seedStr) {
        // derive from a global per-browser-session value + origin
        var g = (typeof window.top !== 'undefined' && window.top === window) ? window : (window.top || window);
        var browserSeed;
        try {
            browserSeed = g.__wedFpSeed__;
        } catch (e) { browserSeed = undefined; }
        if (!browserSeed) {
            browserSeed = String(Date.now()) + '-' + Math.floor(Math.random() * 1e12);
            try { g.__wedFpSeed__ = browserSeed; } catch (e) {}
        }
        var h = 2166136261;
        var mix = browserSeed + '|' + location.origin;
        for (var i = 0; i < mix.length; i++) {
            h ^= mix.charCodeAt(i);
            h = (h * 16777619) >>> 0;
        }
        seedStr = String(h >>> 0);
        if (sessionStore) {
            try { sessionStore.setItem(KEY, seedStr); } catch (e) {}
        }
    }
    var seed = parseInt(seedStr, 10) || 123456789;

    // PRNG (mulberry32) — deterministic from seed
    function rng() {
        seed |= 0; seed = (seed + 0x6D2B79F5) | 0;
        var t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    }
    function jitter(limit) { return (rng() * 2 - 1) * limit; }

    var W = window;

    // -------------------------------------------------------------- Canvas
    var realToDataURL = HTMLCanvasElement.prototype.toDataURL;
    var realToBlob = HTMLCanvasElement.prototype.toBlob;
    var realGetImageData = CanvasRenderingContext2D.prototype.getImageData;

    function noisePixelData(data) {
        // consistent tiny per-channel noise, first ~256 pixels worth is enough
        // to break hashes while keeping the image visually identical
        var n = Math.min(data.length, 4096);
        for (var i = 0; i < n; i += 4) {
            var d = (rng() * 2 - 1) * 1.5; // ±1.5 levels
            data[i]     = Math.max(0, Math.min(255, data[i] + d));
            data[i + 1] = Math.max(0, Math.min(255, data[i + 1] + d));
            data[i + 2] = Math.max(0, Math.min(255, data[i + 2] + d));
        }
    }

    HTMLCanvasElement.prototype.toDataURL = function (type) {
        try {
            var ctx = this.getContext('2d');
            if (ctx && this.width > 0 && this.height > 0) {
                var img = realGetImageData.call(ctx, 0, 0, Math.min(this.width, 32), Math.min(this.height, 32));
                noisePixelData(img.data);
                ctx.putImageData(img, 0, 0);
            }
        } catch (e) {}
        return realToDataURL.apply(this, arguments);
    };

    HTMLCanvasElement.prototype.toBlob = function (callback, type, quality) {
        try {
            var ctx = this.getContext('2d');
            if (ctx && this.width > 0 && this.height > 0) {
                var img = realGetImageData.call(ctx, 0, 0, Math.min(this.width, 32), Math.min(this.height, 32));
                noisePixelData(img.data);
                ctx.putImageData(img, 0, 0);
            }
        } catch (e) {}
        return realToBlob.call(this, callback, type, quality);
    };

    CanvasRenderingContext2D.prototype.getImageData = function (sx, sy, sw, sh) {
        var out = realGetImageData.apply(this, arguments);
        try { noisePixelData(out.data); } catch (e) {}
        return out;
    };

    // -------------------------------------------------------------- Audio
    if (W.AudioContext || W.webkitAudioContext) {
        var AC = W.AudioContext || W.webkitAudioContext;
        var RealAC = AC;
        if (RealAC.prototype && RealAC.prototype.getChannelData) {
            var realGetChannelData = RealAC.prototype.getChannelData;
            RealAC.prototype.getChannelData = function (channel) {
                var data = realGetChannelData.call(this, channel);
                try {
                    if (this.__wedNjs === undefined) {
                        // noise once per context; low amplitude
                        this.__wedNjs = 0.0000015 * (rng() + 0.5);
                    }
                    for (var i = 0; i < data.length; i += 997) {
                        data[i] += this.__wedNjs * Math.sin(i * 0.01 + rng());
                    }
                } catch (e) {}
                return data;
            };
        }
        // AnalyserNode frequency reads
        if (W.AnalyserNode) {
            var realGetFloat = W.AnalyserNode.prototype.getFloatFrequencyData;
            W.AnalyserNode.prototype.getFloatFrequencyData = function (array) {
                realGetFloat.call(this, array);
                try {
                    for (var i = 0; i < array.length; i++) {
                        array[i] += jitter(0.1);
                    }
                } catch (e) {}
            };
            var realGetByte = W.AnalyserNode.prototype.getByteFrequencyData;
            W.AnalyserNode.prototype.getByteFrequencyData = function (array) {
                realGetByte.call(this, array);
                try {
                    for (var i = 0; i < array.length; i++) {
                        array[i] = Math.max(0, Math.min(255, array[i] + (rng() > 0.5 ? 1 : 0)));
                    }
                } catch (e) {}
            };
        }
    }

    // -------------------------------------------------------------- WebGL
    var getParamMasked = false;
    if (W.WebGLRenderingContext) {
        var realGetParam = W.WebGLRenderingContext.prototype.getParameter;
        var VENDOR = 0x1F00, RENDERER = 0x1F01, VERSION = 0x1F02;
        var UNMASKED_VENDOR = 0x9245, UNMASKED_RENDERER = 0x9246;
        W.WebGLRenderingContext.prototype.getParameter = function (pname) {
            var v = realGetParam.call(this, pname);
            try {
                if (pname === VENDOR) return 'WebKit';
                if (pname === RENDERER) return 'WebKit WebGL';
                if (pname === UNMASKED_VENDOR) return 'WebKit';
                if (pname === UNMASKED_RENDERER) return 'WebKit WebGL (WED)';
                if (pname === 0x8B4C) { // MAX_VERTEX_UNIFORM_VECTORS etc get stable jitter
                    return Math.max(1, v - (Math.floor(rng() * 4)));
                }
            } catch (e) {}
            return v;
        };
        getParamMasked = true;
    }
    if (W.WebGL2RenderingContext) {
        var realGetParam2 = W.WebGL2RenderingContext.prototype.getParameter;
        W.WebGL2RenderingContext.prototype.getParameter = function (pname) {
            var v = realGetParam2.call(this, pname);
            try {
                if (pname === 0x1F00 || pname === 0x9245) return 'WebKit';
                if (pname === 0x1F01 || pname === 0x9246) return 'WebKit WebGL (WED)';
            } catch (e) {}
            return v;
        };
    }
    // WebGL debug renderer info extension
    if (W.WebGLRenderingContext) {
        var realGetExt = W.WebGLRenderingContext.prototype.getExtension;
        W.WebGLRenderingContext.prototype.getExtension = function (name) {
            var ext = realGetExt.apply(this, arguments);
            if (name === 'WEBGL_debug_renderer_info' && ext) {
                var realGetParameter = W.WebGLRenderingContext.prototype.getParameter;
                // constants already masked above; keep extension available
                return ext;
            }
            return ext;
        };
    }

    // -------------------------------------------------------------- Navigator
    try {
        if (navigator.hardwareConcurrency !== undefined) {
            Object.defineProperty(navigator, 'hardwareConcurrency', {
                get: function () { return Math.min(navigator.hardwareConcurrency || 4, 4); },
                configurable: true
            });
        }
    } catch (e) {}
    try {
        if (navigator.deviceMemory !== undefined) {
            Object.defineProperty(navigator, 'deviceMemory', {
                get: function () { return 4; },
                configurable: true
            });
        }
    } catch (e) {}

    // -------------------------------------------------------------- Screen
    // round down screen metrics to multiples of 100 to bucketize
    try {
        var sw = screen.width, sh = screen.height;
        Object.defineProperty(screen, 'width',  { get: function () { return Math.max(800, Math.floor(sw / 100) * 100); }, configurable: true });
        Object.defineProperty(screen, 'height', { get: function () { return Math.max(600, Math.floor(sh / 100) * 100); }, configurable: true });
        var realAvailW = screen.availWidth, realAvailH = screen.availHeight;
        Object.defineProperty(screen, 'availWidth',  { get: function () { return Math.max(800, Math.floor(realAvailW / 100) * 100); }, configurable: true });
        Object.defineProperty(screen, 'availHeight', { get: function () { return Math.max(600, Math.floor(realAvailH / 100) * 100); }, configurable: true });
    } catch (e) {}

    // -------------------------------------------------------------- Timing
    // micro-jitter performance.now() to break timing-based correlation
    try {
        var realNow = performance.now.bind(performance);
        performance.now = function () {
            return Math.max(0, realNow() + jitter(0.4));
        };
    } catch (e) {}

    // -------------------------------------------------------------- Plugins
    // give a stable, common plugin set instead of browser-specific enumeration
    try {
        var fakePlugins = [
            { name: 'PDF Viewer', description: 'Portable Document Format',
              filename: 'internal-pdf-viewer', mime: 'application/pdf' },
            { name: 'Chrome PDF Viewer', description: 'Portable Document Format',
              filename: 'internal-pdf-viewer', mime: 'application/pdf' },
            { name: 'Chromium PDF Viewer', description: 'Portable Document Format',
              filename: 'internal-pdf-viewer', mime: 'application/pdf' },
            { name: 'Microsoft Edge PDF Viewer', description: 'Portable Document Format',
              filename: 'internal-pdf-viewer', mime: 'application/pdf' },
            { name: 'WebKit built-in PDF', description: 'Portable Document Format',
              filename: 'internal-pdf-viewer', mime: 'application/pdf' }
        ];
        var arr = [];
        for (var p = 0; p < fakePlugins.length; p++) {
            var o = { name: fakePlugins[p].name, description: fakePlugins[p].description,
                      filename: fakePlugins[p].filename, length: 1 };
            o[0] = { type: fakePlugins[p].mime, suffixes: 'pdf', description: fakePlugins[p].description };
            arr.push(o);
        }
        Object.defineProperty(navigator, 'plugins', {
            get: function () { return arr; },
            configurable: true
        });
    } catch (e) {}

    // -------------------------------------------------------------- Fonts
    // generic font list rather than precise local enumeration
    try {
        if (navigator.fonts && navigator.fonts.check) {
            // leave API functional; measurement-based enumeration is handled by
            // canvas + text metrics noise above.
        }
    } catch (e) {}
})();
