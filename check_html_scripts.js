const fs = require('fs');
const vm = require('vm');
const path = process.argv[2] || 'index.html';
const html = fs.readFileSync(path, 'utf8');
const blocks = [...html.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/gi)].map(m => m[1]);
if (!blocks.length) throw new Error('Geen scriptblokken gevonden');
for (const [i, source] of blocks.entries()) new vm.Script(source, { filename: `${path}:script-${i + 1}` });
console.log(JSON.stringify({ status: 'ok', scriptsChecked: blocks.length, path }));
