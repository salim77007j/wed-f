// adblock_compare.mjs — count requests to known ad/tracker domains on BBC News
// via Playwright (Chromium, no extensions — like stock Chrome), for the
// competitive comparison against WED's Rust filter core.
import { chromium } from 'playwright';

const AD_DOMAINS = [
  'doubleclick.net', 'googlesyndication.com', 'google-analytics.com',
  'permutive.com', 'cxense.com', 'optimizely.com', 'contextweb.com',
  'adsystem.com', 'adnxs.com', 'rubiconproject.com', 'criteo.com',
  'taboola.com', 'outbrain.com', 'scorecardresearch.com', 'moatads.com',
  'amazon-adsystem.com', 'adservice.google.com', 'pubmatic.com',
  'casalemedia.com', 'openx.net', 'smartadserver.com', 'zedo.com',
  'adsrvr.org', '2mdn.net', 'voicefive.com', 'chartbeat.com', 'krxd.net',
];

const run = async () => {
  const browser = await chromium.launch({
    executablePath: '/home/z/.cache/ms-playwright/chromium-1243/chrome-linux64/chrome',
    headless: true,
  });
  const page = await browser.newPage();
  let total = 0, adRelated = 0, blocked = 0;
  const adHosts = {};
  page.on('request', (req) => {
    total++;
    const u = req.url();
    for (const d of AD_DOMAINS) {
      if (u.includes(d)) {
        adRelated++;
        adHosts[d] = (adHosts[d] || 0) + 1;
        break;
      }
    }
  });
  const t0 = Date.now();
  await page.goto('https://www.bbc.com/news', { waitUntil: 'load', timeout: 60000 });
  await page.waitForTimeout(8000);
  const elapsed = (Date.now() - t0) / 1000;
  console.log(JSON.stringify({
    engine: 'chromium-headless-stock',
    page: 'https://www.bbc.com/news',
    totalRequests: total,
    adTrackerRequests: adRelated,
    blockedByDefault: 0,
    adHosts,
    loadSeconds: elapsed.toFixed(2),
  }, null, 1));
  await browser.close();
};
run().catch((e) => { console.error('ERR', e.message); process.exit(1); });
