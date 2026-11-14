// Golf Dynamics — the multiplayer relay: a WebSocket server that forwards each
// message to the others in its room. It keeps no game state (a round is its
// configuration and shot inputs, which the players' pages hold); it only
// tells each room who is in it. No dependencies: Node's http and crypto.
// SPDX-License-Identifier: MIT
//
//   node tools/relay/server.mjs [port]        (default $PORT or 8787)
//   clients: wss://host/?room=<id>&peer=<id>
//   a client sends {from, to?, data}; the others in the room receive it as is
//   (to: one peer only); everyone receives {type: "peers", peers: [...]} on joins and leaves.
//
// Limits: 16 peers a room, 64 KiB a message, rooms vanish when empty.
import http from 'node:http';
import crypto from 'node:crypto';

const PORT = Number(process.argv[2] || process.env.PORT || 8787);
const MAX_PEERS = 16, MAX_MSG = 64 * 1024;
const ORIGINS = (process.env.ALLOWED_ORIGINS || '').split(',').map((s) => s.trim()).filter(Boolean);
const rooms = new Map();  // room -> Map(peer -> socket)

const server = http.createServer((req, res) => {
  res.writeHead(200, { 'content-type': 'text/plain', 'access-control-allow-origin': '*' });
  res.end(`golf relay: ${rooms.size} rooms\n`);
});

function frame(text) {
  const p = Buffer.from(text);
  const n = p.length;
  const head = n < 126 ? Buffer.from([0x81, n]) : n < 65536 ? Buffer.from([0x81, 126, n >> 8, n & 255]) : null;
  if (!head) {
    const h = Buffer.alloc(10);
    h[0] = 0x81; h[1] = 127; h.writeBigUInt64BE(BigInt(n), 2);
    return Buffer.concat([h, p]);
  }
  return Buffer.concat([head, p]);
}

function broadcastPeers(room) {
  const r = rooms.get(room);
  if (!r) return;
  const msg = frame(JSON.stringify({ type: 'peers', peers: [...r.keys()] }));
  for (const s of r.values()) s.write(msg);
}

server.on('upgrade', (req, socket) => {
  const url = new URL(req.url, 'http://x');
  const room = (url.searchParams.get('room') || '').slice(0, 40);
  const peer = (url.searchParams.get('peer') || '').slice(0, 40);
  const key = req.headers['sec-websocket-key'];
  const origin = req.headers.origin || '';
  if (!room || !peer || !key || (ORIGINS.length && !ORIGINS.includes(origin))) { socket.destroy(); return; }
  const accept = crypto.createHash('sha1').update(key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  socket.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${accept}\r\n\r\n`);
  if (!rooms.has(room)) rooms.set(room, new Map());
  const r = rooms.get(room);
  if (r.size >= MAX_PEERS && !r.has(peer)) { socket.end(); return; }
  const old = r.get(peer);
  if (old && old !== socket) old.destroy();
  r.set(peer, socket);
  broadcastPeers(room);
  let buf = Buffer.alloc(0), fragments = [];
  const leave = () => {
    if (r.get(peer) === socket) { r.delete(peer); if (!r.size) rooms.delete(room); else broadcastPeers(room); }
  };
  socket.on('data', (chunk) => {
    buf = Buffer.concat([buf, chunk]);
    while (buf.length >= 2) {
      const fin = buf[0] & 0x80, op = buf[0] & 0x0f, masked = buf[1] & 0x80;
      let len = buf[1] & 0x7f, off = 2;
      if (len === 126) { if (buf.length < 4) return; len = buf.readUInt16BE(2); off = 4; }
      else if (len === 127) { if (buf.length < 10) return; len = Number(buf.readBigUInt64BE(2)); off = 10; }
      if (len > MAX_MSG) { socket.destroy(); return; }
      const mask = masked ? buf.subarray(off, off + 4) : null;
      if (masked) off += 4;
      if (buf.length < off + len) return;
      const payload = Buffer.from(buf.subarray(off, off + len));
      if (mask) for (let i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];
      buf = buf.subarray(off + len);
      if (op === 0x8) { socket.end(); return; }
      if (op === 0x9) { socket.write(Buffer.concat([Buffer.from([0x8a, payload.length]), payload])); continue; }
      if (op === 0x1 || op === 0x0) {
        fragments.push(payload);
        if (!fin) continue;
        const text = Buffer.concat(fragments).toString('utf8');
        fragments = [];
        let to = null;
        try { to = JSON.parse(text).to || null; } catch { continue; }
        const out = frame(text);
        for (const [id, s] of r) if (id !== peer && (!to || to === id)) s.write(out);
      }
    }
  });
  socket.on('close', leave);
  socket.on('error', leave);
});

server.listen(PORT, () => console.log(`golf relay on :${PORT}`));
