let wasmInstance;

async function initWasm() {
    const response = await fetch('sniper.wasm');
    const bytes = await response.arrayBuffer();
    const { instance } = await WebAssembly.instantiate(bytes);
    wasmInstance = instance;
}

function bigIntToWasm(bi, ptr) {
    const view = new BigUint64Array(wasmInstance.exports.memory.buffer, ptr, 4);
    for (let i = 0; i < 4; i++) {
        view[i] = bi & 0xFFFFFFFFFFFFFFFFn;
        bi >>= 64n;
    }
}

// Helper for Base58 (needed for WIF)
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

// Simple SHA256 for WIF checksum (can use Wasm later if needed)
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
    if (ev.data.type === "ADDRESS_SCAN") {
        if (!wasmInstance) await initWasm();

        const { startHex, endHex, targetH160Hex, workerId, targetAddress } = ev.data;
        const startKey = BigInt("0x" + startHex);
        const endKey = BigInt("0x" + endHex);
        const targetH160 = new Uint8Array(targetH160Hex.match(/.{2}/g).map(b => parseInt(b, 16)));

        // Use the end of the memory for our temporary variables
        // We'll just pick a safe spot like 64KB in
        const memBase = 65536; 
        const startXPtr = memBase;
        const startYPtr = startXPtr + 32;
        const startZPtr = startYPtr + 32;
        const targetPtr = startZPtr + 32;
        const foundIdxPtr = targetPtr + 32;

        bigIntToWasm(startKey, startXPtr);
        wasmInstance.exports.get_point_from_scalar(startXPtr, startXPtr, startYPtr, startZPtr);

        const targetBuf = new Uint8Array(wasmInstance.exports.memory.buffer, targetPtr, 20);
        targetBuf.set(targetH160);

        let currentKey = startKey;
        const batchSize = 2048; // Max batch size defined in C

        while (currentKey <= endKey) {
            const foundIdx = wasmInstance.exports.scan_batch(startXPtr, startYPtr, startZPtr, targetPtr, batchSize, foundIdxPtr);
            
            if (foundIdx !== -1) {
                const foundKeyIdx = new BigUint64Array(wasmInstance.exports.memory.buffer, foundIdxPtr, 1)[0];
                const finalKey = currentKey + foundKeyIdx;
                const privHex = finalKey.toString(16).padStart(64, '0');
                const wif = await generateWIF(privHex);
                
                self.postMessage({ type: "FOUND", privHex: privHex, wif: wif, address: targetAddress });
                return;
            }

            currentKey += BigInt(batchSize);
            self.postMessage({ type: "PROGRESS", count: batchSize, workerId });
            
            // Yield to event loop occasionally
            if (Number(currentKey % (BigInt(batchSize) * 100n)) === 0) {
                await new Promise(r => setTimeout(r, 0));
            }
        }
        
        self.postMessage({ type: "SCAN_DONE", workerId });
    }
};
