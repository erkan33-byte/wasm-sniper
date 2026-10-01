// Plakt native_jacobian.wasm (base64) in template.html -> gsmg-scanner.html (één bestand).
const fs=require('fs');
const t=fs.readFileSync('template.html','utf8'),w=fs.readFileSync('native_jacobian.wasm').toString('base64');
fs.writeFileSync('gsmg-scanner.html',t.replace('__WASM_B64__',w));
console.log('gsmg-scanner.html geschreven ('+Math.round(w.length/1024)+' KB WASM ingebed)');
