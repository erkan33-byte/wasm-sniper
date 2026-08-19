let wasmPromise = null;
async function wasmApi() {
  if (!wasmPromise) wasmPromise = (async () => {
    const r = await fetch('fast_p2pkh_v2.wasm?build=20260819');
    if (!r.ok) throw new Error('WASM HTTP ' + r.status);
    const imports = {
      __wbindgen_placeholder__: {
        __wbindgen_describe: () => {},
        __wbg___wbindgen_throw_344f42d3211c4765: () => { throw new Error('WASM-bindgen fout'); }
      },
      __wbindgen_externref_xform__: {
        __wbindgen_externref_table_set_null: () => {},
        __wbindgen_externref_table_grow: () => 0
      }
    };
    const {instance} = await WebAssembly.instantiate(await r.arrayBuffer(), imports);
    const e = instance.exports;
    for (const name of ['memory', 'init_sniper', 'scan_batch']) if (!(name in e)) throw new Error('Ontbrekende WASM-export: ' + name);
    return e;
  })();
  return wasmPromise;
}
function base58Decode(address) {
  const alpha = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz';
  const digits = [0];
  for (const ch of address) {
    let carry = alpha.indexOf(ch);
    if (carry < 0) throw new Error('Ongeldig Base58-teken');
    for (let i = 0; i < digits.length; i++) { carry += digits[i] * 58; digits[i] = carry & 255; carry >>>= 8; }
    while (carry) { digits.push(carry & 255); carry >>>= 8; }
  }
  for (let i = 0; i < address.length && address[i] === '1'; i++) digits.push(0);
  digits.reverse();
  if (digits.length !== 25 || digits[0] !== 0) throw new Error('Alleen standaard P2PKH-versie 00 is toegestaan');
  return new Uint8Array(digits.slice(1, 21));
}
function writeU256BE(heap, ptr, value) {
  let x = value;
  for (let i = 31; i >= 0; i--) { heap[ptr + i] = Number(x & 255n); x >>= 8n; }
}
function hex256(x) { return x.toString(16).padStart(64, '0'); }
self.onmessage = async ({data}) => {
  if (data.type !== 'scan') return;
  try {
    const E = await wasmApi();
    const heap = new Uint8Array(E.memory.buffer);
    const startPtr = 1024, targetPtr = 1060, foundPtr = 1100;
    let key = BigInt('0x' + data.startHex), end = BigInt('0x' + data.endHex);
    writeU256BE(heap, startPtr, key);
    heap.set(base58Decode(data.address), targetPtr);
    E.init_sniper(startPtr);
    self.postMessage({type:'ready'});
    const batchMax = 10000;
    while (key <= end) {
      const count = Number((end - key + 1n) > BigInt(batchMax) ? BigInt(batchMax) : (end - key + 1n));
      const result = E.scan_batch(targetPtr, count, foundPtr);
      if (result === 0) {
        const offset = new Uint32Array(E.memory.buffer, foundPtr, 1)[0];
        if (offset < count) { self.postMessage({type:'found', key:hex256(key + BigInt(offset)), count:offset + 1}); return; }
      }
      key += BigInt(count);
      self.postMessage({type:'progress', count});
    }
    self.postMessage({type:'done'});
  } catch (e) {
    self.postMessage({type:'error', error:String(e && e.message || e)});
  }
};
