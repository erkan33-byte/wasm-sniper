/* Native secp256k1/Jacobian P2PKH worker.
 * De invoerbuffers liggen bewust boven de vaste arrays van de WASM-kern.
 */
let wasmPromise = null;
let cachedAddress = '';
let cachedTargetHash = null;

const START_PTR = 2000000;
const TARGET_PTR = 2000032;
const FOUND_PTR = 2000064;
const BATCH_SIZE = 10000;
const BATCH_SIZE_N = 10000n;
const PROGRESS_EVERY = 100000;

async function wasmApi() {
  if (!wasmPromise) {
    wasmPromise = (async () => {
      const response = await fetch('native_jacobian.wasm?build=20260822-maxexact-combined-fold-simd');
      if (!response.ok) throw new Error('WASM HTTP ' + response.status);
      const { instance } = await WebAssembly.instantiate(await response.arrayBuffer(), {});
      const e = instance.exports;
      for (const name of ['memory', 'init_sniper', 'scan_batch']) {
        if (!(name in e)) throw new Error('Ontbrekende native WASM-export: ' + name);
      }
      if (e.memory.buffer.byteLength <= FOUND_PTR + 4) {
        throw new Error('Native WASM-geheugen is te klein voor veilige invoerbuffers');
      }
      return { e, heap: new Uint8Array(e.memory.buffer), view: new DataView(e.memory.buffer) };
    })();
  }
  return wasmPromise;
}

function base58DecodeP2PKH(address) {
  const alphabet = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz';
  const digits = [0];
  for (const ch of address) {
    let carry = alphabet.indexOf(ch);
    if (carry < 0) throw new Error('Ongeldig Base58-teken');
    for (let i = 0; i < digits.length; i++) {
      carry += digits[i] * 58;
      digits[i] = carry & 255;
      carry >>>= 8;
    }
    while (carry) {
      digits.push(carry & 255);
      carry >>>= 8;
    }
  }
  for (let i = 0; i < address.length && address[i] === '1'; i++) digits.push(0);
  digits.reverse();
  if (digits.length !== 25 || digits[0] !== 0) {
    throw new Error('Alleen een standaard Bitcoin P2PKH-adres (versie 00) is toegestaan');
  }
  return new Uint8Array(digits.slice(1, 21));
}

function targetHashFor(address) {
  if (address !== cachedAddress) { cachedAddress = address; cachedTargetHash = base58DecodeP2PKH(address); }
  return cachedTargetHash;
}

function writeU256BE(heap, ptr, value) {
  let x = value;
  for (let i = 31; i >= 0; i--) {
    heap[ptr + i] = Number(x & 255n);
    x >>= 8n;
  }
}

function hex256(value) {
  return value.toString(16).padStart(64, '0');
}

self.onmessage = async ({ data }) => {
  if (data.type !== 'scan') return;
  const token = data.token;
  try {
    const runtime = await wasmApi();
    const E = runtime.e;
    const heap = runtime.heap;
    const view = runtime.view;
    let key = BigInt('0x' + data.startHex);
    const end = BigInt('0x' + data.endHex);
    let reported = 0;
    writeU256BE(heap, START_PTR, key);
    heap.set(targetHashFor(data.address), TARGET_PTR);
    view.setUint32(FOUND_PTR, 0, true);
    E.init_sniper(START_PTR);
    self.postMessage({ type: 'ready', token });
    while (key <= end) {
      const remaining = end - key + 1n;
      const count = Number(remaining > BATCH_SIZE_N ? BATCH_SIZE_N : remaining);
      const result = E.scan_batch(TARGET_PTR, count, FOUND_PTR);
      if (result === 0) {
        const offset = view.getUint32(FOUND_PTR, true);
        if (offset < count) {
          self.postMessage({ type:'found', token, key:hex256(key + BigInt(offset)), count:reported + offset + 1 });
          return;
        }
        throw new Error('Native WASM gaf een ongeldige matchoffset terug');
      }
      if (result !== -1) throw new Error('Onverwachte native resultaatcode: ' + result);
      key += BigInt(count);
      reported += count;
      if (reported >= PROGRESS_EVERY) { self.postMessage({ type:'progress', token, count:reported }); reported = 0; }
    }
    if (reported) self.postMessage({ type:'progress', token, count:reported });
    self.postMessage({ type:'done', token });
  } catch (e) {
    self.postMessage({ type:'error', token, error:String(e && e.message || e) });
  }
};
