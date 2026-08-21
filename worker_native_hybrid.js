let wasmPromise = null;
const START_PTR = 2000000, TARGET_PTR = 2000032, FOUND_PTR = 2000064, BATCH_SIZE = 10000;

async function wasmApi() {
  if (!wasmPromise) wasmPromise = (async () => {
    const r = await fetch('native_jacobian.wasm?build=20260820');
    if (!r.ok) throw new Error('WASM HTTP ' + r.status);
    const {instance} = await WebAssembly.instantiate(await r.arrayBuffer(), {});
    const e = instance.exports;
    for (const k of ['memory','init_sniper','scan_batch']) if (!(k in e)) throw new Error('Ontbrekende WASM-export: '+k);
    if (e.memory.buffer.byteLength <= FOUND_PTR + 4) throw new Error('WASM-geheugen is te klein');
    return e;
  })();
  return wasmPromise;
}
function targetHash(address) {
  const alpha='123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz',digits=[0];
  for(const ch of address){let carry=alpha.indexOf(ch);if(carry<0)throw Error('Ongeldig Base58-teken');for(let i=0;i<digits.length;i++){carry+=digits[i]*58;digits[i]=carry&255;carry>>>=8;}while(carry){digits.push(carry&255);carry>>>=8;}}
  for(let i=0;i<address.length&&address[i]==='1';i++)digits.push(0);digits.reverse();
  if(digits.length!==25||digits[0]!==0)throw Error('Alleen standaard P2PKH-versie 00 is toegestaan');
  return new Uint8Array(digits.slice(1,21));
}
function writeU256BE(heap,ptr,x){for(let i=31;i>=0;i--){heap[ptr+i]=Number(x&255n);x>>=8n;}}
function hex64(x){return x.toString(16).padStart(64,'0');}

self.onmessage=async({data})=>{
  if(data.type!=='scan')return;
  const token=data.token;
  try{
    const E=await wasmApi(),heap=new Uint8Array(E.memory.buffer);
    let key=BigInt('0x'+data.startHex),end=BigInt('0x'+data.endHex),reported=0;
    writeU256BE(heap,START_PTR,key);heap.set(targetHash(data.address),TARGET_PTR);new DataView(E.memory.buffer).setUint32(FOUND_PTR,0,true);E.init_sniper(START_PTR);
    self.postMessage({type:'ready',token});
    while(key<=end){const count=Number((end-key+1n)>BigInt(BATCH_SIZE)?BigInt(BATCH_SIZE):end-key+1n);const rc=E.scan_batch(TARGET_PTR,count,FOUND_PTR);if(rc===0){const offset=new DataView(E.memory.buffer).getUint32(FOUND_PTR,true);if(offset<count){self.postMessage({type:'found',token,key:hex64(key+BigInt(offset)),count:reported+offset+1});return;}throw Error('Ongeldige native matchoffset');}if(rc!==-1)throw Error('Onverwachte native resultaatcode: '+rc);key+=BigInt(count);reported+=count;if(reported>=100000){self.postMessage({type:'progress',token,count:reported});reported=0;}}
    if(reported)self.postMessage({type:'progress',token,count:reported});self.postMessage({type:'done',token});
  }catch(e){self.postMessage({type:'error',token,error:String(e&&e.message||e)});}
};
