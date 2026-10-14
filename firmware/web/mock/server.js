#!/usr/bin/env node
// Mock do portal do aparelho sobre uma pasta temporaria, sem dependencias.
// --dir e a raiz da midia; as playlists ficam em <dir>/music e os .mp3 soltos em <dir>/music e <dir>/.
// Uso: node server.js [--port=8080] [--rate=200k] [--free=3M] [--drop=0.1] [--lang=en] [--latency] [--fix=<pasta>] [--page=<html>] [--dir=<pasta>]
//   [--seed=N]  cria N playlists com 3 faixas sinteticas cada (cenarios de 1, 5 e 16 playlists)
'use strict';
const fs = require('fs');
const http = require('http');
const os = require('os');
const path = require('path');

const arg = {};
process.argv.slice(2).forEach(a => {
  const m = /^--([^=]+)(?:=(.*))?$/.exec(a);
  if (m) arg[m[1]] = m[2] === undefined ? true : m[2];
});
const bytes = v => {
  const m = /^([\d.]+)([kKmM]?)$/.exec(String(v));
  return m ? Math.round(+m[1] * ({ k: 1024, m: 1048576 }[m[2].toLowerCase()] || 1)) : 0;
};

const PORT = +(arg.port || 8080);
const RATE = arg.rate ? bytes(arg.rate) : 0;
const TOTAL = bytes(arg.free || '3M');
const DROP = +(arg.drop || 0);
const LANG = arg.lang || 'pt-BR';
const FIX = arg.fix ? path.resolve(arg.fix) : null;
const RESERVE = 65536;
const LIM = { pl: 250, tr: 1000, plLen: 24, trLen: 48, file: 1 << 30 }; // igual ao firmware: Library.h e Portal.h
const MAX_LOOSE = 64;
const MEDIA = arg.dir ? path.resolve(arg.dir) : fs.mkdtempSync(path.join(os.tmpdir(), 'mrm-mock-'));
const ROOT = path.join(MEDIA, 'music');
fs.mkdirSync(ROOT, { recursive: true });
// Duas frames MPEG1 Layer III 128 kbps 44,1 kHz bastam para o sniff aceitar.
function seed(n) {
  const f = Buffer.alloc(417);
  f[0] = 0xff; f[1] = 0xfb; f[2] = 0x90;
  const names = ['rock', 'viagem', 'mpb', 'jazz', 'treino', 'samba', 'indie', 'clássicos', 'forró', 'lo-fi', 'pop 2000', 'bossa nova',
    'trilhas de filmes', 'podcast', 'infantil', 'acústico ao vivo'];
  for (let i = 0; i < n; i++) {
    const d = path.join(ROOT, names[i % names.length] + (i >= names.length ? ' ' + i : ''));
    fs.mkdirSync(d, { recursive: true });
    for (let k = 1; k <= 3; k++) fs.writeFileSync(path.join(d, `0${k} - faixa ${k}.mp3`), Buffer.concat([f, f]));
  }
}
if (arg.seed) seed(+arg.seed);
const PAGE = arg.page ? path.resolve(arg.page) : path.join(__dirname, '..', 'portal.min.html');
let last = 'ok', lu = 0;

// Mesmas regras do PathRules.h, contando bytes como o strlen do aparelho.
function validName(s, max) {
  const n = s ? Buffer.byteLength(s) : 0;
  return n > 0 && n <= max && !/^[ .]|[ .]$/.test(s) && !/[\x00-\x1f\x7f/\\:*?"<>|]/.test(s);
}
const validPl = s => validName(s, LIM.plLen);
const validTr = s => !!s && Buffer.byteLength(s) > 4 && validName(s, LIM.trLen) && /\.mp3$/i.test(s);

// Ordem do aparelho: sem diferenciar caixa, desempate pelos bytes.
const order = (a, b) => (a.toLowerCase() < b.toLowerCase() ? -1 : a.toLowerCase() > b.toLowerCase() ? 1 : a < b ? -1 : a > b ? 1 : 0);
const playlists = () =>
  fs.readdirSync(ROOT, { withFileTypes: true }).filter(d => d.isDirectory() && validPl(d.name)).map(d => d.name).sort(order);
const tracks = p =>
  fs.readdirSync(path.join(ROOT, p), { withFileTypes: true }).filter(d => d.isFile() && validTr(d.name)).map(d => d.name).sort(order)
    .map(n => ({ n, b: fs.statSync(path.join(ROOT, p, n)).size }));
const looseIn = (dir, s) => fs.readdirSync(dir, { withFileTypes: true }).filter(d => d.isFile() && validTr(d.name)).map(d => d.name).sort(order)
  .map(n => ({ n, b: fs.statSync(path.join(dir, n)).size, s }));
const loose = () => looseIn(ROOT, 'music').concat(looseIn(MEDIA, 'root'));
const walk = d => fs.readdirSync(d, { withFileTypes: true }).reduce((s, e) =>
  s + (e.isDirectory() ? walk(path.join(d, e.name)) : e.name.startsWith('.') ? 0 : fs.statSync(path.join(d, e.name)).size), 0);
const used = () => walk(MEDIA);
const free = () => TOTAL - used();

function state() {
  const pl = playlists().map(n => {
    const t = tracks(n);
    return { n, c: t.length, b: t.reduce((a, x) => a + x.b, 0) };
  });
  return { lang: LANG, name: 'marmota mp3 K7QX', free: free(), total: TOTAL, reserve: RESERVE, lim: LIM, pl, last, lu };
}

const BR = [[0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320], [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160]];
function frame(b, i) {
  if (b[i] != 0xff || (b[i + 1] & 0xe0) != 0xe0) return null;
  const ver = (b[i + 1] >> 3) & 3, br = b[i + 2] >> 4, si = (b[i + 2] >> 2) & 3;
  if (ver == 1 || ((b[i + 1] >> 1) & 3) != 1 || br == 0 || br == 15 || si == 3) return null;
  const sr = [44100, 48000, 32000][si] / (ver == 3 ? 1 : ver == 2 ? 2 : 4);
  return { sr, len: Math.floor((ver == 3 ? 144 : 72) * BR[ver == 3 ? 0 : 1][br] * 1000 / sr) + ((b[i + 2] >> 1) & 1) };
}

// Sniff em streaming como o Mp3Sniff do aparelho: pula ID3 sem guardar, decide em ate 64 KB.
function sniffer() {
  let skip = 0, win = Buffer.alloc(0), result = null;
  return (chunk, end) => {
    if (result !== null) return result;
    const k = Math.min(skip, chunk.length);
    skip -= k;
    win = Buffer.concat([win, chunk.subarray(k)]);
    while (win.length >= 10 && win.toString('latin1', 0, 3) == 'ID3') {
      const s = 10 + (win[6] << 21 | win[7] << 14 | win[8] << 7 | win[9]) + (win[5] & 0x10 ? 10 : 0);
      if (win.length >= s) win = win.subarray(s);
      else { skip = s - win.length; win = Buffer.alloc(0); }
    }
    if (win.length < 65536 && !end) return null;
    const b = win.subarray(0, 65536);
    result = 0;
    for (let i = 0; i + 4 <= b.length && !result; i++) {
      const h = frame(b, i);
      if (h && (i + h.len + 4 > b.length || frame(b, i + h.len))) result = h.sr;
    }
    return result;
  };
}

function json(res, code, obj) {
  res.writeHead(code, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
  res.end(JSON.stringify(obj));
}

// O aparelho responde e fecha o socket sem ler o resto do corpo.
function refuse(req, res, code, e) {
  last = e;
  console.log('  recusa', e);
  res.setHeader('Connection', 'close');
  json(res, code, { e });
  res.on('finish', () => setTimeout(() => req.socket.destroy(), 20));
}

function upload(req, res, u) {
  const p = u.searchParams.get('p'), n = u.searchParams.get('n');
  const len = +req.headers['content-length'] || 0;
  lu = +u.searchParams.get('u') || 0;
  console.log(`upload ${p}/${n} ${len} B (${u.searchParams.get('i')}/${u.searchParams.get('of')})`);
  if (req.headers['content-type'] != 'application/octet-stream') return refuse(req, res, 415, 'io');
  if (!validPl(p) || !validTr(n)) return refuse(req, res, 400, 'name');
  const dir = path.join(ROOT, p), isNew = !fs.existsSync(dir);
  if (len <= 0 || len > LIM.file) return refuse(req, res, 413, 'limit');
  if (isNew && playlists().length >= LIM.pl) return refuse(req, res, 409, 'limit');
  if (!isNew && tracks(p).length >= LIM.tr) return refuse(req, res, 409, 'limit');
  if (!isNew && fs.existsSync(path.join(dir, n))) return refuse(req, res, 409, 'exists'); // antes do espaco, como o UploadSink
  if (len > free() - RESERVE) return refuse(req, res, 507, 'space');

  const tmp = path.join(ROOT, '.up.tmp');
  const out = fs.openSync(tmp, 'w');
  const sniff = sniffer();
  const t0 = Date.now(), dropAt = Math.random() < DROP ? Math.floor(Math.random() * len) : -1;
  let got = 0, over = false;
  const abort = (e, code) => {
    over = true;
    fs.closeSync(out);
    fs.rmSync(tmp, { force: true });
    if (code) return refuse(req, res, code, e);
    last = e;
    console.log('  derruba a conexao em', got);
    req.socket.destroy();
  };
  req.on('data', c => {
    if (over) return;
    got += c.length;
    fs.writeSync(out, c);
    if (dropAt >= 0 && got >= dropAt) return abort('aborted');
    if (sniff(c, false) === 0) return abort('notmp3', 415);
    if (RATE) {
      const wait = got / RATE * 1000 - (Date.now() - t0);
      if (wait > 0) { req.pause(); setTimeout(() => req.resume(), wait); }
    }
  });
  req.on('aborted', () => { if (!over) { over = true; fs.closeSync(out); fs.rmSync(tmp, { force: true }); last = 'aborted'; } });
  req.on('end', () => {
    if (over) return;
    over = true;
    fs.closeSync(out);
    const sr = sniff(Buffer.alloc(0), true);
    if (got != len) { fs.rmSync(tmp, { force: true }); return refuse(req, res, 400, 'io'); }
    if (!sr) { fs.rmSync(tmp, { force: true }); return refuse(req, res, 415, 'notmp3'); }
    fs.mkdirSync(dir, { recursive: true });
    fs.renameSync(tmp, path.join(dir, n));
    last = 'ok';
    json(res, 200, { ok: 1, sr, free: free() });
  });
}

const numPrefix = n => (/^\d+ - /.exec(n) || [''])[0];

// Troca os prefixos "NN - " e mantem os titulos; sem prefixo nos dois (ou iguais) nao troca.
function swap(res, u) {
  const p = u.searchParams.get('p'), a = u.searchParams.get('a'), b = u.searchParams.get('b');
  if (!validPl(p) || !validTr(a) || !validTr(b) || a == b) return json(res, 400, { e: 'name' });
  if (!fs.existsSync(path.join(ROOT, p, a)) || !fs.existsSync(path.join(ROOT, p, b))) return json(res, 404, { e: 'name' });
  const pa = numPrefix(a), pb = numPrefix(b);
  if (!pa || !pb || pa == pb) return json(res, 400, { e: 'num' });
  const na = pb + a.slice(pa.length), nb = pa + b.slice(pb.length);
  if (!validTr(na) || !validTr(nb)) return json(res, 400, { e: 'name' });
  const has = n => fs.existsSync(path.join(ROOT, p, n));
  if ((has(na) && na != b) || (has(nb) && nb != a)) return json(res, 409, { e: 'exists' });
  const tmp = path.join(ROOT, p, '.' + na);
  fs.renameSync(path.join(ROOT, p, a), tmp);
  fs.renameSync(path.join(ROOT, p, b), path.join(ROOT, p, nb));
  fs.renameSync(tmp, path.join(ROOT, p, na));
  json(res, 200, { ok: 1 });
}

// p vazio com n: .mp3 solto, em src=music (padrao) ou src=root. Devolve o caminho ou null.
function source(u, p, n) {
  if (p !== '') return validPl(p) ? path.join(ROOT, p, n || '') : null;
  const s = u.searchParams.get('src') || 'music';
  return n && (s == 'music' || s == 'root') ? path.join(s == 'root' ? MEDIA : ROOT, n) : null;
}

function move(res, u, p, n, src) {
  const to = u.searchParams.get('to');
  if (!n || !validPl(to)) return json(res, 400, { e: 'name' });
  const dir = path.join(ROOT, to), isNew = !fs.existsSync(dir);
  if (!isNew && fs.existsSync(path.join(dir, n))) return json(res, 409, { e: 'exists' });
  if (isNew ? playlists().length >= LIM.pl : tracks(to).length >= LIM.tr) return json(res, 409, { e: 'limit' });
  fs.mkdirSync(dir, { recursive: true });
  fs.renameSync(src, path.join(dir, n));
  if (p && !tracks(p).length) try { fs.rmdirSync(path.join(ROOT, p)); } catch (e) {}
  json(res, 200, { ok: 1 });
}

function post(req, res, u) {
  const p = u.searchParams.get('p'), n = u.searchParams.get('n'), to = u.searchParams.get('to');
  if (u.pathname == '/api/done') { console.log('done: sai do modo'); return json(res, 200, { ok: 1 }); }
  if (u.pathname == '/api/swap') return swap(res, u);
  if (p === null || (n !== null && !validTr(n))) return json(res, 400, { e: 'name' });
  const src = source(u, p, n);
  if (!src) return json(res, 400, { e: 'name' });
  if (!fs.existsSync(src)) return json(res, 404, { e: 'name' });
  if (u.pathname == '/api/move') return move(res, u, p, n, src);
  if (u.pathname == '/api/delete') {
    fs.rmSync(src, { recursive: true });
    return json(res, 200, { ok: 1, free: free() });
  }
  if (u.pathname == '/api/rename') {
    if (n ? !validTr(to) : !validPl(to)) return json(res, 400, { e: 'name' });
    const dst = path.join(src, '..', to);
    if (fs.existsSync(dst)) return json(res, 409, { e: 'exists' });
    fs.renameSync(src, dst);
    return json(res, 200, { ok: 1 });
  }
  json(res, 404, { e: 'io' });
}

function fixture(res, rel) {
  const list = (d, pre = '') => fs.readdirSync(d, { withFileTypes: true }).flatMap(e =>
    e.isDirectory() ? list(path.join(d, e.name), pre + e.name + '/') : [pre + e.name]);
  if (!rel) return json(res, 200, list(FIX));
  const f = path.join(FIX, rel);
  if (!f.startsWith(FIX) || !fs.existsSync(f)) return json(res, 404, {});
  res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
  res.end(fs.readFileSync(f));
}

http.createServer(async (req, res) => {
  const u = new URL(req.url, 'http://x');
  if (u.pathname.startsWith('/api/') && arg.latency) await new Promise(r => setTimeout(r, 100 + Math.random() * 500));
  try {
    if (req.method == 'GET' && u.pathname == '/') {
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' });
      return res.end(fs.readFileSync(PAGE));
    }
    if (req.method == 'GET' && FIX && u.pathname.startsWith('/fix/')) return fixture(res, decodeURIComponent(u.pathname.slice(5)));
    if (req.method == 'GET' && u.pathname == '/api/state') return json(res, 200, state());
    if (req.method == 'GET' && u.pathname == '/api/loose') {
      const t = loose();
      return json(res, 200, { t: t.slice(0, MAX_LOOSE), more: t.length > MAX_LOOSE ? 1 : 0 });
    }
    if (req.method == 'GET' && u.pathname == '/api/tracks') {
      const p = u.searchParams.get('p');
      if (!validPl(p) || !fs.existsSync(path.join(ROOT, p))) return json(res, 404, { e: 'name' });
      return json(res, 200, { p, t: tracks(p) });
    }
    if (req.method == 'POST' && u.pathname.startsWith('/api/')) {
      if (req.headers['x-mrm'] != '1') return refuse(req, res, 403, 'io');
      return u.pathname == '/api/upload' ? upload(req, res, u) : post(req, res, u);
    }
    json(res, 404, { e: 'io' });
  } catch (e) {
    console.error(e);
    json(res, 500, { e: 'io' });
  }
}).listen(PORT, () => console.log(`mock em http://localhost:${PORT} sobre ${ROOT}`));
