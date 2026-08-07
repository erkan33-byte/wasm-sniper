let wasmInstance;

async function loadWasm() {
    const response = await fetch('fast_sni_final.wasm');
    const buffer = await response.arrayBuffer();
    const obj = await WebAssembly.instantiate(buffer, {});
    wasmInstance = obj.instance;
}

function toBase58(hex) {
    const ALPHA = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz';
    let bytes = hex.match(/.{2}/g).map(b => parseInt(b, 16));
    let digits = [0];
    for (let i = 0; i < bytes.length; i++) {
        let carry = bytes[i];
        for (let j = 0; j < digits.length; j++) {
            carry += digits[j] << 8;
            digits[j] = carry % 58;
            carry = (carry / 58) | 0;
        }
        while (carry > 0) {
            digits.push(carry % 58);
            carry = (carry / 58) | 0;
        }
    }
    let res = '';
    for (let i = 0; i < bytes.length && bytes[i] === 0; i++) res += '1';
    for (let i = digits.length - 1; i >= 0; i--) res += ALPHA[digits[i]];
    return res;
}

async function sha256(bytes) {
    const hash = await crypto.subtle.digest('SHA-256', bytes);
    return new Uint8Array(hash);
}

async function generateWIF(privHex) {
    const payload = "80" + privHex + "01";
    const bytes = new Uint8Array(payload.match(/.{2}/g).map(b => parseInt(b, 16)));
    const hash1 = await sha256(bytes);
    const hash2 = await sha256(hash1);
    const checksum = Array.from(hash2.slice(0, 4)).map(b => b.toString(16).padStart(2, '0')).join('');
    return toBase58(payload + checksum);
}

self.onmessage = async function(ev) {
    try {
        if (ev.data.type === "ADDRESS_SCAN") {
            if (!wasmInstance) {
                await loadWasm();
            }

            const { startHex, endHex, targetH160Hex, workerId, targetAddress } = ev.data;
            const startKey = BigInt("0x" + startHex);
            const endKey = BigInt("0x" + endHex);
            const targetH160 = new Uint8Array(targetH160Hex.match(/.{2}/g).map(b => parseInt(b, 16)));

            const memory = wasmInstance.exports.memory;
            const startKeyPtr = 1000000;
            const targetH160Ptr = 1000040;
            const foundOffsetPtr = 1000070;
            
            while (memory.buffer.byteLength < foundOffsetPtr + 100) {
                memory.grow(1);
            }

            const heap = new Uint8Array(memory.buffer);
            const startKeyBytes = new Uint8Array(32);
            let tempKey = startKey;
            for (let i = 31; i >= 0; i--) {
                startKeyBytes[i] = Number(tempKey & 0xFFn);
                tempKey >>= 8n;
            }
            
            heap.set(startKeyBytes, startKeyPtr);
            heap.set(targetH160, targetH160Ptr);

            wasmInstance.exports.init_sniper(startKeyPtr);

            let currentKey = startKey;
            const batchSize = 10000;

            while (currentKey <= endKey) {
                const res = wasmInstance.exports.scan_batch(targetH160Ptr, batchSize, foundOffsetPtr);
                
                if (res === 0) {
                    const foundOffset = new Uint32Array(memory.buffer, foundOffsetPtr, 1)[0];
                    const finalKey = currentKey + BigInt(foundOffset);
                    const privHex = finalKey.toString(16).padStart(64, '0');
                    const wif = await generateWIF(privHex);
                    self.postMessage({ type: "FOUND", privHex: privHex, wif: wif, address: targetAddress });
                    break;
                }

                currentKey += BigInt(batchSize);
                self.postMessage({ type: "PROGRESS", count: batchSize, workerId });
                
                if (currentKey % BigInt(batchSize * 50) === 0n) {
                    await new Promise(r => setTimeout(r, 0));
                }
            }
            
            self.postMessage({ type: "SCAN_DONE", workerId });
        }
    } catch (err) {
        self.postMessage({ type: "ERROR", message: err.toString() });
    }
};
