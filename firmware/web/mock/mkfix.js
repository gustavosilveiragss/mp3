#!/usr/bin/env node
// Gera MP3 sinteticos para o mock e para o teste no navegador. Uso: node mkfix.js <pasta>
'use strict';
const fs = require('fs');
const path = require('path');

const out = process.argv[2] || path.join(__dirname, 'fix');

function id3(size) {
  const h = Buffer.alloc(10 + size);
  h.write('ID3', 0);
  h[3] = 4;
  [21, 14, 7, 0].forEach((s, i) => (h[6 + i] = (size >> s) & 0x7f));
  return h;
}

// MPEG1 Layer III 128 kbps sem CRC; 44,1 kHz da frames de 417 B, 48 kHz de 384 B.
function frames(rate, bytes) {
  const len = rate == 48000 ? 384 : 417;
  const f = Buffer.alloc(len);
  f[0] = 0xff;
  f[1] = 0xfb;
  f[2] = rate == 48000 ? 0x94 : 0x90;
  return Buffer.concat(Array(Math.ceil(bytes / len)).fill(f));
}

function write(rel, ...parts) {
  const p = path.join(out, rel);
  fs.mkdirSync(path.dirname(p), { recursive: true });
  fs.writeFileSync(p, Buffer.concat(parts));
}

const kb = n => n * 1024;
write('solta 44k.mp3', id3(512), frames(44100, kb(300)));
write('solta 48k.mp3', id3(512), frames(48000, kb(200)));
write('lixo.mp3', Buffer.from('isto nao e mp3\n'.repeat(5000)));
write('id3 grande.mp3', id3(kb(1024)), frames(44100, kb(100)));
write('nome: "proibido"? e muito longo, passando de quarenta e oito bytes.mp3', id3(64), frames(44100, kb(60)));
write('Café com acento NFD.mp3', id3(64), frames(44100, kb(60)));
for (let i = 1; i <= 6; i++) write(`album teste/${i} - faixa ${i}.mp3`, id3(128), frames(44100, kb(150 + i * 10)));
write('album teste/capa.jpg', Buffer.from('jpg'));
console.log('fixtures em ' + out);
