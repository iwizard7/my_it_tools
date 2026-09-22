const assert = require('node:assert/strict');

function slugify(value) {
  return value.toLowerCase().normalize('NFKD').replace(/[\u0300-\u036f]/g, '')
    .replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
}

function ipv4Network(ip, prefix) {
  const parts = ip.split('.').map(Number);
  const value = parts.reduce((a, x) => (a << 8) | x, 0) >>> 0;
  const mask = prefix === 0 ? 0 : (0xffffffff << (32 - prefix)) >>> 0;
  const network = (value & mask) >>> 0;
  return [network >>> 24, network >>> 16 & 255, network >>> 8 & 255, network & 255].join('.');
}

assert.equal(slugify('Hello, World!'), 'hello-world');
assert.equal(slugify('Développeur ESP32'), 'developpeur-esp32');
assert.equal(ipv4Network('192.168.1.42', 24), '192.168.1.0');
console.log('JavaScript tool tests passed');
