importScripts("sniper.js"); // Import the Emscripten-generated glue code

let Module;

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
        if (!Module) {
            Module = await createSniperModule();
            Module._init_wasm(); // Initialize G_table and other Wasm-side data
            console.log("Wasm Module initialized.");
        }

        const { startHex, endHex, targetH160Hex, workerId, targetAddress } = ev.data;
        const startKey = BigInt("0x" + startHex);
        const endKey = BigInt("0x" + endHex);
        const targetH160 = new Uint8Array(targetH160Hex.match(/.{2}/g).map(b => parseInt(b, 16)));

        // Allocate memory in Wasm heap for arguments
        const startXPtr = Module._malloc(32);
        const startYPtr = Module._malloc(32);
        const startZPtr = Module._malloc(32);
        const targetPtr = Module._malloc(20);
        const foundIdxPtr = Module._malloc(8); // uint64_t

        // Convert startKey to Wasm-compatible format and get initial point
        const scalarView = new BigUint64Array(Module.HEAPU8.buffer, startXPtr, 4);
        let tempKey = startKey;
        for (let i = 0; i < 4; i++) {
            scalarView[i] = tempKey & 0xFFFFFFFFFFFFFFFFn;
            tempKey >>= 64n;
        }
        Module._get_point_from_scalar(startXPtr, startXPtr, startYPtr, startZPtr);

        // Set target H160
        Module.HEAPU8.set(targetH160, targetPtr);

        let currentKey = startKey;
        const batchSize = 2048; // Max batch size defined in C

        while (currentKey <= endKey) {
            const foundIdx = Module._scan_batch(startXPtr, startYPtr, startZPtr, targetPtr, batchSize, foundIdxPtr);
            
            if (foundIdx !== -1) {
                const foundKeyOffset = new BigUint64Array(Module.HEAPU8.buffer, foundIdxPtr, 1)[0];
                const finalKey = currentKey + foundKeyOffset;
                const privHex = finalKey.toString(16).padStart(64, '0');
                const wif = await generateWIF(privHex);
                
                self.postMessage({ type: "FOUND", privHex: privHex, wif: wif, address: targetAddress });
                break; // Exit loop after finding a match
            }

            currentKey += BigInt(batchSize);
            self.postMessage({ type: "PROGRESS", count: batchSize, workerId });
            
            // Yield to event loop occasionally
            if (Number(currentKey % (BigInt(batchSize) * 100n)) === 0) {
                await new Promise(r => setTimeout(r, 0));
            }
        }
        
        self.postMessage({ type: "SCAN_DONE", workerId });

        // Free allocated memory
        Module._free(startXPtr);
        Module._free(startYPtr);
        Module._free(startZPtr);
        Module._free(targetPtr);
        Module._free(foundIdxPtr);
    }
};
