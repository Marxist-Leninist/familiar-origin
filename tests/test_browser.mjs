// Drive the browser VM page in a real Chromium-based browser (Microsoft Edge, Chrome or
// Chromium) through the DevTools protocol and write evidence/browser-qa.json.
//
//   node tests/test_browser.mjs
//
// Environment:
//   FO_URL      page to test, default http://127.0.0.1:8767/ (serve browser-site/dist there)
//   FO_BROWSER  browser executable, default: Edge on Windows, else chromium/chrome on PATH dirs
//   FO_REPORT   report path, default evidence/browser-qa.json
//   FO_BROWSER_ARGS  extra browser flags, space separated (for example --no-sandbox when
//               Chromium runs as root in a container)
//
// Needs Node 22 or newer (built-in WebSocket and fetch). No packages. The browser runs
// headless with a throwaway profile, which is deleted afterwards.
import { spawn } from 'node:child_process';
import crypto from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const base = (process.env.FO_URL || 'http://127.0.0.1:8767/').replace(/\/?$/, '/');
const report = process.env.FO_REPORT || 'evidence/browser-qa.json';
const browser = process.env.FO_BROWSER || [
  'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
  'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
  '/usr/bin/microsoft-edge', '/usr/bin/chromium', '/usr/bin/chromium-browser', '/usr/bin/google-chrome',
].find(p => fs.existsSync(p));
if (!browser) { console.error('No browser found. Set FO_BROWSER.'); process.exit(2); }

const sleep = ms => new Promise(r => setTimeout(r, ms));
const port = 9300 + crypto.randomInt(600);
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'fo-browser-'));
const child = spawn(browser, ['--headless=new', `--remote-debugging-port=${port}`, '--remote-allow-origins=*',
  `--user-data-dir=${profile}`, '--no-first-run', '--no-default-browser-check', '--disable-extensions',
  '--disable-background-networking', '--mute-audio', ...(process.env.FO_BROWSER_ARGS || '').split(' ').filter(Boolean),
  'about:blank'], { stdio: 'ignore' });

const checks = [];
const problems = [];
const origin = new URL(base).origin;
const otherOrigins = new Set();
const failedRequests = [];
// Requests to the page's own host but outside the site folder. The site makes none, so
// a non-zero count means something between the browser and the server added them, for
// example security software that filters browser traffic. tls_issuer shows who signed
// the certificate the browser actually saw.
let sameHostOutsideSite = 0;
let tlsIssuer = '';
let cspViolations = [];
let failure = null;
let product = '', userAgent = '';

class Session {
  constructor(ws) {
    this.ws = ws; this.next = 0; this.pending = new Map(); this.onEvent = () => {};
    ws.addEventListener('message', event => {
      const message = JSON.parse(event.data);
      if (message.id) {
        const waiter = this.pending.get(message.id); this.pending.delete(message.id);
        if (message.error) waiter.reject(new Error(message.error.message)); else waiter.resolve(message.result);
      } else this.onEvent(message);
    });
  }
  send(method, params = {}) {
    const id = ++this.next;
    this.ws.send(JSON.stringify({ id, method, params }));
    return new Promise((resolve, reject) => this.pending.set(id, { resolve, reject }));
  }
}

async function connect() {
  for (let i = 0; i < 100; i++) {
    try {
      const version = await (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
      product = version.Browser; userAgent = version['User-Agent'];
      const target = await (await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, { method: 'PUT' })).json();
      const ws = new WebSocket(target.webSocketDebuggerUrl);
      await new Promise((resolve, reject) => { ws.addEventListener('open', resolve); ws.addEventListener('error', reject); });
      return new Session(ws);
    } catch { await sleep(200); }
  }
  throw new Error('Browser did not open its DevTools port.');
}

async function main() {
  const s = await connect();
  s.onEvent = message => {
    if (message.method === 'Network.requestWillBeSent') {
      const url = message.params.request.url;
      if (!/^(blob:|data:|about:)/.test(url)) {
        if (new URL(url).origin !== origin) otherOrigins.add(url);
        else if (!url.startsWith(base)) sameHostOutsideSite++;
      }
    } else if (message.method === 'Network.responseReceived') {
      const response = message.params.response;
      if (response.status >= 400) failedRequests.push(response.status + ' ' + response.url);
      if (response.securityDetails && message.params.type === 'Document') tlsIssuer = response.securityDetails.issuer;
    } else if (message.method === 'Runtime.exceptionThrown') {
      problems.push('exception: ' + (message.params.exceptionDetails.exception?.description || message.params.exceptionDetails.text));
    } else if (message.method === 'Log.entryAdded' && message.params.entry.level === 'error') {
      problems.push('console error: ' + message.params.entry.text);
    }
  };
  await s.send('Page.enable'); await s.send('Runtime.enable'); await s.send('Log.enable'); await s.send('Network.enable');
  await s.send('Page.addScriptToEvaluateOnNewDocument', { source:
    "window.__csp=[];document.addEventListener('securitypolicyviolation',e=>window.__csp.push(e.violatedDirective+' '+e.blockedURI));" });

  const js = async expression => {
    const result = await s.send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
    if (result.exceptionDetails) throw new Error(result.exceptionDetails.exception?.description || result.exceptionDetails.text);
    return result.result.value;
  };
  const until = async (expression, what, ms = 30000) => {
    const end = Date.now() + ms;
    while (Date.now() < end) { if (await js(expression)) return; await sleep(150); }
    throw new Error('Timed out waiting for: ' + what);
  };
  const pass = (name, ok = true) => { if (!ok) throw new Error('Check failed: ' + name); checks.push(name); };
  const text = id => js(`document.getElementById(${JSON.stringify(id)}).textContent`);
  const click = id => js(`document.getElementById(${JSON.stringify(id)}).click()`);
  const run = command => js(`(()=>{const c=document.getElementById('command');c.value=${JSON.stringify(command)};document.getElementById('command-form').requestSubmit();})()`);
  const terminalHas = value => `document.getElementById('terminal').textContent.includes(${JSON.stringify(value)})`;
  const ready = "document.getElementById('status').textContent==='Kernel ready'";
  const open = async page => {
    await s.send('Page.navigate', { url: base + page });
    await until("document.readyState==='complete'", page + ' to load');
  };
  const collectCsp = async () => { cspViolations = cspViolations.concat(await js('window.__csp||[]')); };

  // ---- the VM page -------------------------------------------------------------------
  await open('');
  await until("typeof V86==='function'&&typeof OriginCommands==='object'", 'page scripts');
  pass('Page shows version 0.1.1', (await js("document.querySelector('.brand span').textContent")) === '0.1.1');
  pass('Command table lists all 16 rows before the VM starts', (await js("document.querySelectorAll('.command-table tbody tr').length")) === 16);
  await click('start');
  await until(ready, 'kernel ready after Start VM', 120000);
  const boot = await text('terminal');
  pass('v86 boots the image to the kernel prompt', boot.includes('FAMILIAR ORIGIN 0.1.1'));
  pass('Kernel boot self-test passes', boot.includes('BOOT SELFTEST PASS'));
  pass('Native framebuffer canvas is displayed', await js("(()=>{const c=document.querySelector('#screen canvas');return !!c&&getComputedStyle(c).display!=='none'&&c.width>0;})()"));

  await run('show my files');
  await until(terminalHas('welcome.txt'), 'file listing');
  pass('Plain phrase "show my files" lists RAM files');
  await run('shwo my files');
  await until("document.getElementById('interpretation').textContent.startsWith('Corrected')", 'typo correction');
  pass('Typo "shwo my files" is corrected to ls and shown as a correction');

  await click('make-file');
  await until("document.getElementById('file-dialog').open", 'file dialog');
  pass('File creator suggests the next unused name file1.txt', (await js("document.getElementById('filename').value")) === 'file1.txt');
  await js("(()=>{document.getElementById('file-text').value='written from the browser test';document.getElementById('file-form').requestSubmit();})()");
  await until("document.getElementById('operation-status').textContent.includes('Created file1.txt')", 'file creation');
  pass('File created in guest RAM through the kernel new command');
  await run('read file1.txt');
  await until(terminalHas('written from the browser test'), 'file read back');
  pass('Plain phrase "read file1.txt" returns the stored text');

  await run('delete file1.txt');
  await until("document.getElementById('action-dialog').open", 'delete confirmation');
  pass('Delete names its target and asks first', (await text('action-detail')).includes('rm file1.txt'));
  await click('cancel-action');
  await click('refresh-files');
  await sleep(800);
  pass('Cancel keeps the file', (await text('file-list')).includes('file1.txt'));
  await run('delete file1.txt');
  await until("document.getElementById('action-dialog').open", 'delete confirmation');
  await click('confirm-action');
  await until("!document.getElementById('file-list').textContent.includes('file1.txt')", 'file removal');
  pass('Confirm removes the file and refreshes the listing');

  await click('pause');
  await until("document.getElementById('status').textContent.startsWith('Paused')", 'pause');
  await click('start');
  await until(ready, 'resume');
  await run('cpu');
  await until(terminalHas('32-bit protected mode'), 'command after resume');
  pass('Pause and resume keep the guest running');

  await run('new keep.txt stays-until-restart');
  await until("document.getElementById('file-list').textContent.includes('keep.txt')", 'keep.txt');
  await click('restart');
  await until("document.getElementById('reset-dialog').open", 'restart dialog');
  await click('cancel-reset');
  await run('r keep.txt');
  await until(terminalHas('stays-until-restart'), 'file survives a cancelled restart');
  pass('Cancelled restart keeps RAM files');
  await click('restart');
  await until("document.getElementById('reset-dialog').open", 'restart dialog');
  await click('confirm-reset');
  await until(ready + "&&document.getElementById('terminal').textContent.includes('BOOT SELFTEST PASS')", 'reboot', 120000);
  await click('refresh-files');
  await sleep(1000);
  pass('Confirmed restart reboots the guest and clears RAM files', !(await text('file-list')).includes('keep.txt'));
  await collectCsp();

  // ---- the Transformer lab -----------------------------------------------------------
  await open('hello.html');
  await click('train');
  await until("document.getElementById('training-status').textContent.startsWith('Training complete')", 'training', 240000);
  pass('Transformer worker reaches 250 steps', (await js("document.getElementById('training-progress').value")) === 250);
  pass('Trained model predicts "Hello, world!"', (await text('prediction')) === 'Hello, world!');
  await collectCsp();

  // ---- licences ----------------------------------------------------------------------
  await open('licenses.html');
  const links = await js("[...document.querySelectorAll('a[href]')].map(a=>a.href)");
  for (const link of links) {
    const reply = await fetch(link, { method: 'HEAD' });
    if (reply.status !== 200) throw new Error('Licence page link failed: ' + link + ' -> ' + reply.status);
  }
  pass('All ' + links.length + ' links on the licences page answer 200');
  await collectCsp();

  pass('No request went to another origin', otherOrigins.size === 0);
  pass('Every request was answered without an error status', failedRequests.length === 0);
  pass('No Content-Security-Policy violation', cspViolations.length === 0);
  pass('No uncaught exception or console error', problems.length === 0);
}

try { await main(); } catch (error) { failure = String(error.message || error); }
child.kill();
await sleep(500);
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* the profile is in the temp folder */ }

let image = '';
try { image = crypto.createHash('sha256').update(Buffer.from(await (await fetch(base + 'assets/origin.img')).arrayBuffer())).digest('hex'); } catch { image = 'unavailable'; }
const result = {
  status: failure ? 'FAIL' : 'PASS', version: '0.1.1', tested_at_utc: new Date().toISOString(), url: base,
  browser: product, user_agent: userAgent, mode: 'headless, throwaway profile', host: `${os.type()} ${os.release()} ${os.arch()}`,
  image_sha256: image, checks, failure,
  other_origin_requests: [...otherOrigins], same_host_requests_outside_site: sameHostOutsideSite, tls_issuer: tlsIssuer || 'not HTTPS',
  failed_requests: failedRequests, csp_violations: cspViolations, page_errors: problems,
  scope: 'One automated run in one browser. Not a hardware, performance or security test. The guest mouse path is tested under QEMU in boot-test.json, not here.',
  produced_by: 'tests/test_browser.mjs',
};
fs.writeFileSync(report, JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify(result, null, 2));
process.exit(failure ? 1 : 0);
