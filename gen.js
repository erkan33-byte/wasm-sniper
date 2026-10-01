const fs=require('fs');
const s=fs.readFileSync('/mnt/user-data/outputs/puzzle-02btc.html','utf8');
const js=s.match(/<script>([\s\S]*)<\/script>/)[1];
const core=js.slice(0,js.indexOf('const $='));
const L=new Function(core+";return {mul,sha,rmd160,hex,unhex,cat}")();
const hash160=async k=>{const [x,y]=L.mul(k);const X=x.toString(16).padStart(64,"0");const pub=L.unhex((y&1n?"03":"02")+X);return L.hex(L.rmd160(await L.sha(pub)))};
(async()=>{const K=0x40A3D70A3D70A3D701n+777n,K2=0x4FFFFFFFFFFFFFFF00n;
 const cases=[[K,700],[K,0],[K,1024],[K,3*1025+5],[K2,512],[K2,2000],[0x2832ed74f2b5e35een,100]];
 for(const [k,d] of cases){console.log((k-BigInt(d)).toString(16).padStart(64,"0"),await hash160(k),d)}})();
