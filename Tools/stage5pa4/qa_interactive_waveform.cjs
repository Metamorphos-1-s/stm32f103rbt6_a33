#!/usr/bin/env node
// Browser smoke test for the generated standalone waveform (needs Playwright).

const assert = require("node:assert/strict");
const path = require("node:path");
const { pathToFileURL } = require("node:url");
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || "playwright");

async function main() {
  const target = process.argv[2];
  const screenshot = process.argv[3];
  if (!target) throw new Error("Usage: node qa_interactive_waveform.cjs PLOT.html [SCREENSHOT.png]");
  const browser = await chromium.launch({ channel: "msedge", headless: true });
  try {
    const context = await browser.newContext({ viewport: { width: 1280, height: 800 } });
    const page = await context.newPage();
    const errors = [];
    page.on("pageerror", error => errors.push(error.message));
    await page.goto(pathToFileURL(path.resolve(target)).href);
    const canvas = page.locator("#waveform");
    await canvas.waitFor();
    const all = await page.locator("#range").innerText();
    assert.match(all, /85\.4 分钟/);
    assert.match(await page.locator("#meta").innerText(), /10,244/);
    assert.match(await page.locator("#warning").innerText(), /首次装载边沿未被记录/);
    const box = await canvas.boundingBox();
    const centerX = box.x + box.width / 2;
    const centerY = box.y + box.height / 3;
    await page.mouse.move(centerX, centerY);
    await page.mouse.wheel(0, -320);
    const zoomed = await page.locator("#range").innerText();
    assert.notEqual(zoomed, all, "wheel zoom must change time range");
    await page.mouse.move(centerX, centerY);
    const hover = await page.locator("#readout").innerText();
    assert.match(hover, /权威/);
    assert.match(hover, /raw/);
    await page.mouse.down();
    await page.mouse.move(centerX + 140, centerY, { steps: 6 });
    await page.mouse.up();
    assert.notEqual(await page.locator("#range").innerText(), zoomed,
                    "drag must pan time range");
    await page.getByRole("button", { name: "放大" }).click();
    await page.getByRole("button", { name: "缩小" }).click();
    await page.locator("input[data-series=display]").uncheck();
    await page.locator("input[data-series=display]").check();
    await page.locator("#markers").uncheck();
    await page.locator("#markers").check();
    await page.getByRole("button", { name: "全时段" }).click();
    assert.equal(await page.locator("#range").innerText(), all,
                 "full-range reset must restore exact original view");
    const fit = await page.evaluate(() => {
      const chart = document.querySelector("#waveform").getBoundingClientRect();
      const status = document.querySelector("#readout").getBoundingClientRect();
      return { chartHeight: chart.height, chartBottom: chart.bottom,
               statusBottom: status.bottom, h: innerHeight, scrollX:
               document.documentElement.scrollWidth > innerWidth };
    });
    assert.ok(fit.chartHeight >= 210 && fit.statusBottom <= fit.h && !fit.scrollX,
              JSON.stringify(fit));
    if (screenshot) {
      await page.screenshot({ path: screenshot, scale: "css" });
      const focus = box.x + box.width * .39;
      await page.mouse.move(focus, centerY);
      for (let n = 0; n < 4; n++) await page.mouse.wheel(0, -320);
      await page.screenshot({ path: screenshot.replace(/\.png$/i, "_zoom.png"), scale: "css" });
      await page.getByRole("button", { name: "全时段" }).click();
    }
    await page.locator("#quickView").selectOption("empty1");
    assert.match(await page.locator("#range").innerText(), /29\.\d 分钟/);
    await page.locator("#quickView").selectOption("load2");
    const plateau = await page.locator("#range").innerText();
    assert.match(plateau, /32\.\d 分钟/);
    if (screenshot) await page.screenshot({
      path: screenshot.replace(/\.png$/i, "_plateau.png"), scale: "css" });
    await page.locator("#quickView").selectOption("empty2");
    assert.match(await page.locator("#range").innerText(), /18\.\d 分钟/);
    await page.getByRole("button", { name: "全时段" }).click();
    assert.equal(await page.locator("#quickView").inputValue(), "all");
    await page.setViewportSize({ width: 720, height: 620 });
    const narrow = await page.evaluate(() => ({ scrollX:
      document.documentElement.scrollWidth > innerWidth,
      plot: document.querySelector("#waveform").getBoundingClientRect().height,
      statusBottom: document.querySelector("#readout").getBoundingClientRect().bottom,
      h: innerHeight }));
    assert.ok(!narrow.scrollX && narrow.plot >= 210 && narrow.statusBottom <= narrow.h,
              JSON.stringify(narrow));
    if (screenshot) await page.screenshot({
      path: screenshot.replace(/\.png$/i, "_narrow.png"), scale: "css" });
    assert.deepEqual(errors, []);
    console.log(JSON.stringify({ all, zoomed, plateau, hover, fit, narrow, pageErrors: errors }));
    await context.close();
  } finally {
    await browser.close();
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
