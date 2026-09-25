// Verify the running read-only SSE viewer using ordinary browser interactions.
const assert = require("node:assert/strict");
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || "playwright");

async function main() {
  const url = process.argv[2] || "http://127.0.0.1:8765/?realtime=1";
  const screenshot = process.argv[3];
  const browser = await chromium.launch({ channel: "msedge", headless: true });
  try {
    const context = await browser.newContext({ viewport: { width: 1280, height: 800 } });
    const page = await context.newPage();
    const errors = [];
    page.on("pageerror", e => errors.push(e.message));
    await page.goto(url, { waitUntil: "domcontentloaded" });
    await page.locator("#realtimeStatus").waitFor();
    await page.waitForFunction(() => document.querySelector("#meta").textContent.includes("个记录点"), null, { timeout: 10000 });
    await page.waitForFunction(() => document.querySelector("#meta").textContent.includes("2026-09-25"), null, { timeout: 10000 });
    const count = async () => page.evaluate(() => points.length);
    const before = await count();
    await page.waitForFunction(start => points.length >= start + 3, before, { timeout: 10000 });
    const after = await count();
    assert.ok(after > before);
    const canvas = page.locator("#waveform");
    const box = await canvas.boundingBox();
    await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
    await page.mouse.wheel(0, -310);
    assert.equal(await page.locator("#followLive").isChecked(), false,
                 "wheel zoom must pause automatic follow");
    const wheelRange = await page.locator("#range").innerText();
    await page.waitForFunction(start => points.length >= start + 1, after, { timeout: 10000 });
    assert.equal(await page.locator("#range").innerText(), wheelRange,
                 "zoomed window must stay put while new points arrive");
    await page.mouse.down();
    await page.mouse.move(box.x + box.width / 2 + 100, box.y + box.height / 2, {steps:4});
    await page.mouse.up();
    const pausedRange = await page.locator("#range").innerText();
    await page.waitForFunction(start => points.length >= start + 3, after, { timeout: 10000 });
    assert.equal(await page.locator("#range").innerText(), pausedRange,
                 "manual navigation should not auto-follow new points");
    await page.locator("#followLive").check();
    assert.equal(await page.locator("#followLive").isChecked(), true);
    if (screenshot) await page.screenshot({ path: screenshot, scale: "css" });
    await page.reload({ waitUntil: "domcontentloaded" });
    await page.locator("#realtimeStatus").waitFor();
    await page.waitForFunction(min => points.length >= min, after, { timeout: 10000 });
    assert.equal(errors.length, 0, errors.join("; "));
    console.log(JSON.stringify({ initialPoints:before, advancedPoints:after,
      reconnectPoints:await count(), status:await page.locator("#realtimeStatus").innerText(),
      pageErrors:errors }));
    await context.close();
  } finally { await browser.close(); }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
