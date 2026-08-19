/* Native WASM BSGS worker.
 * De baby-step cache is per worker en wordt hergebruikt zolang m gelijk blijft.
 */
let wasmPromise = null;

const PUB_PTR = 24000000;
const START_PTR = 24000064;
const END_PTR = 24000096;
const OUT_PTR = 24000128;
const MAX_M = 262145;

async function wasmApi() {
  if (!wasmPromise) {
    wasmPromise = (async () => {
      const response = await fetch('bsgs_native.wasm?build=20260819');
      if (!response.ok) throw new Error('WASM HTTP ' + response.status);
      const { instance } = await WebAssembly.instantiate(await response.arrayBuffer(), {});
      const e = instance.exports;
      for (const name of ['memory', 'bsgs_prepare', 'bsgs_scan']) {
        if (!(name in e)) throw new Error('Ontbrekende BSGS-WASM-export: ' + name);
      }
      if (e.memory.buffer.byteLength <= OUT_PTR + 32) {
        throw new Error('Native BSGS-WASM-geheugen is te klein');
      }
      return e;
    })();
  }
  return wasmPromise;
}

function cleanHex(value) {
  const h = String(value).trim().replace(/^0x/i, '');
  if (!/^[0-9a-f]+$/i.test(h)) throw new Error('Ongeldige hexwaarde');
  return h;
}

function pubKeyBytes(value) {
  const h = cleanHex(value);
  if (!/^(02|03)[0-9a-f]{64}$/i.test(h)) {
    throw new Error('Gebruik een gecomprimeerde secp256k1-public key van 66 hextekens, beginnend met 02 of 03');
  }
  const out = new Uint8Array(33);
  for (let i = 0; i < 33; i++) out[i] = parseInt(h.slice(i * 2, i * 2 + 2), 16);
  return out;
}

function writeU256BE(heap, ptr, value) {
  let x = value;
  for (let i = 31; i >= 0; i--) {
    heap[ptr + i] = Number(x & 255n);
    x >>= 8n;
  }
}

function chooseM(width) {
  if (width < 1n || width > 68719476736n) {
    throw new Error('Deze BSGS-WASM-versie ondersteunt slices van 1 tot en met 2^36 keys');
  }
  const m = Math.ceil(Math.sqrt(Number(width)));
  if (m > MAX_M) throw new Error('Baby-step-tabel is groter dan de vaste WASM-capaciteit');
  return m;
}

self.onmessage = async ({ data }) => {
  if (data.type !== 'scan') return;
  const token = data.token;
  try {
    const E = await wasmApi();
    const heap = new Uint8Array(E.memory.buffer);
    const start = BigInt('0x' + cleanHex(data.startHex));
    const end = BigInt('0x' + cleanHex(data.endHex));
    if (start > end) throw new Error('Start ligt na einde');
    const width = end - start + 1n;
    const m = chooseM(width);
    const giants = Math.ceil(Number(width) / m);

    heap.set(pubKeyBytes(data.targetPubKey), PUB_PTR);
    writeU256BE(heap, START_PTR, start);
    writeU256BE(heap, END_PTR, end);
    self.postMessage({ type: 'ready', token, m, giants, width: width.toString() });

    const result = E.bsgs_scan(PUB_PTR, START_PTR, END_PTR, m, giants, OUT_PTR);
    if (result === 1) {
      const found = Array.from(heap.slice(OUT_PTR, OUT_PTR + 32), b => b.toString(16).padStart(2, '0')).join('');
      self.postMessage({ type: 'found', token, key: found, scanned: width.toString(), m, giants });
    } else if (result === 0) {
      self.postMessage({ type: 'done', token, scanned: width.toString(), m, giants });
    } else {
      throw new Error('Native BSGS-resultaatcode: ' + result);
    }
  } catch (e) {
    self.postMessage({ type: 'error', token, error: String(e && e.message || e) });
  }
};
