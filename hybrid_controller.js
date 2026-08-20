/* Hybrid controller: WebGPU and native WASM scan disjoint active ranges. */
(() => {
  const originalStart = document.getElementById('startBtn');
  const originalStop = document.getElementById('stopBtn');
  const startBtn = originalStart.cloneNode(true);
  const stopBtn = originalStop.cloneNode(true);
  originalStart.replaceWith(startBtn);
  originalStop.replaceWith(stopBtn);

  let cfg = null, hybridToken = 0, activeLeases = new Map(), cpuWorkers = [], gpuLease = null;
  let cpuScanned = 0, gpuScanned = 0, cpuCompleted = 0, matchFound = false;

  function h(id) { return document.getElementById(id); }
  function asBig(value, label) {
    const text = String(value).trim().replace(/^0x/i, '');
    if (!/^[0-9a-f]+$/i.test(text)) throw Error(label + ' is geen geldige hexwaarde');
    return BigInt('0x' + text);
  }
  function readConfig() {
    const min = asBig(h('customStart').value, 'Beginwaarde');
    const max = asBig(h('customEnd').value, 'Eindwaarde');
    if (min > max) throw Error('Beginwaarde ligt na eindwaarde');
    const gpuKeys = BigInt(h('keysLimit').value);
    const cpuKeys = BigInt(h('cpuKeys').value);
    if (gpuKeys < 1n || cpuKeys < 1n) throw Error('Slicegroottes moeten positief zijn');
    return {
      address: h('btcAddr').value.trim(), min, max, span: max - min + 1n,
      gpuKeys, cpuKeys,
      cpuCount: Math.max(0, Math.min(16, Number(h('cpuWorkers').value) || 0))
    };
  }
  function secureBelow(bound) {
    const top = 1n << 256n, cutoff = top - (top % bound), bytes = new Uint8Array(32);
    for (;;) {
      crypto.getRandomValues(bytes);
      let x = 0n;
      for (const b of bytes) x = (x << 8n) | BigInt(b);
      if (x < cutoff) return x % bound;
    }
  }
  function overlaps(a, b) { return a.start <= b.end && b.start <= a.end; }
  function reserve(label, requested) {
    const len = requested > cfg.span ? cfg.span : requested;
    if (len < 1n) return null;
    const room = cfg.span - len + 1n;
    for (let attempt = 0; attempt < 128; attempt++) {
      const start = cfg.min + secureBelow(room);
      const candidate = { start, end: start + len - 1n, label };
      let collision = false;
      for (const lease of activeLeases.values()) if (overlaps(candidate, lease)) { collision = true; break; }
      if (!collision) { activeLeases.set(label, candidate); return candidate; }
    }
    return null;
  }
  function release(label) { activeLeases.delete(label); }
  function renderLeases() {
    const gpu = gpuLease ? `GPU 0x${gpuLease.start.toString(16)}:0x${gpuLease.end.toString(16)}` : 'GPU wacht';
    const cpu = [];
    for (const [name, lease] of activeLeases) if (name.startsWith('cpu:')) cpu.push(`${name.replace('cpu:','CPU')}:0x${lease.start.toString(16)}:0x${lease.end.toString(16)}`);
    h('rangeStart').value = gpuLease ? '0x' + gpuLease.start.toString(16) : '';
    h('rangeEnd').value = gpuLease ? '0x' + gpuLease.end.toString(16) : '';
    h('cpuStatus').value = cpu.length ? cpu.join(' | ') : 'Geen actieve CPU-range';
    log(`Hybride leases: ${gpu}; ${cpu.length ? cpu.join(' | ') : 'geen CPU-range'}`, 'hop');
  }
  function updateHybridCounters() {
    keysCheckedGlobal = gpuScanned + cpuScanned;
    keysCheckedSlice = gpuLease ? Math.min(gpuScanned, Number(gpuLease.end - gpuLease.start + 1n)) : 0;
  }
  async function reportMatch(privHex, source) {
    if (matchFound) return;
    matchFound = true;
    const wifStr = await wifCompressed(hexToBytes(privHex));
    h('noMatchRow').style.display = 'none';
    h('matchTable').innerHTML += `<tr><td>P2PKH (${source})</td><td style="color:var(--accent2);">${wifStr}</td><td style="color:var(--success);">0x${privHex}</td></tr>`;
    log(`🎯 BINGO (${source}): 0x${privHex}`, 'success');
    stopHybrid(`MATCH (${source})`);
  }
  function stopHybrid(reason = 'Gestopt') {
    isRunning = false;
    hybridToken++;
    for (const w of cpuWorkers) if (w) w.terminate();
    cpuWorkers = [];
    activeLeases.clear();
    gpuLease = null;
    h('startBtn').disabled = false;
    h('stopBtn').disabled = true;
    h('statusVal').textContent = reason;
    h('statusVal').className = 'value';
    h('cpuStatus').value = reason;
    updateHybridCounters();
    h('keysScanned').textContent = keysCheckedGlobal.toLocaleString('nl-NL');
    const elapsed = (performance.now() - startTime) / 1000;
    h('speed').textContent = elapsed > 0 ? Math.round(keysCheckedGlobal / elapsed).toLocaleString('nl-NL') + ' /s' : '0 /s';
    log(reason, 'stop');
  }
  function startCpuWorker(id, token) {
    if (!isRunning || token !== hybridToken) return;
    const label = 'cpu:' + id;
    const lease = reserve(label, cfg.cpuKeys);
    if (!lease) { h('cpuStatus').value = 'CPU wacht: geen vrije niet-overlappende range'; return; }
    renderLeases();
    const worker = new Worker('worker_native_hybrid.js?build=20260820');
    cpuWorkers[id] = worker;
    worker.onmessage = ({data}) => {
      if (!isRunning || token !== hybridToken) return;
      if (data.type === 'progress') { cpuScanned += Number(data.count); updateHybridCounters(); }
      else if (data.type === 'done') { release(label); cpuCompleted++; updateHybridCounters(); startCpuWorker(id, token); }
      else if (data.type === 'found') { cpuScanned += Number(data.count); updateHybridCounters(); reportMatch(data.key, 'native WASM'); }
      else if (data.type === 'error') { release(label); log(`CPU-worker ${id + 1} fout: ${data.error}`, 'warn'); h('cpuStatus').value = `CPU-worker ${id + 1} fout`; }
    };
    worker.onerror = e => { if (isRunning) { release(label); log(`CPU-worker ${id + 1} crash: ${e.message}`, 'warn'); } };
    worker.postMessage({ type:'scan', token, address:cfg.address, startHex:lease.start.toString(16), endHex:lease.end.toString(16) });
  }
  async function setupGpu() {
    if (!gpuDevice) {
      const ok = await initWebGPU();
      if (!ok) return false;
    }
    const targetBytes = getTargetHash160Bytes(cfg.address);
    const target = new Uint32Array(8);
    for (let i=0;i<5;i++) target[i] = targetBytes[i*4] | (targetBytes[i*4+1]<<8) | (targetBytes[i*4+2]<<16) | (targetBytes[i*4+3]<<24);
    gpuDevice.queue.writeBuffer(uniformsBuffer, 0, target);
    if (pipelineStages.length === 0) {
      zeroRes = new Uint32Array([0,0]);
      for (let i=0;i<PIPELINE_DEPTH;i++) {
        const bBuf=gpuDevice.createBuffer({size:96,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});
        const rsBuf=gpuDevice.createBuffer({size:8,usage:GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_SRC|GPUBufferUsage.COPY_DST});
        const rdBuf=gpuDevice.createBuffer({size:8,usage:GPUBufferUsage.COPY_DST|GPUBufferUsage.MAP_READ});
        const bg=gpuDevice.createBindGroup({layout:gpuPipeline.getBindGroupLayout(0),entries:[{binding:0,resource:{buffer:bBuf}},{binding:1,resource:{buffer:tableBuffer}},{binding:2,resource:{buffer:uniformsBuffer}},{binding:3,resource:{buffer:rsBuf}}]});
        pipelineStages.push({batchBuffer:bBuf,resBuf:rsBuf,readBuf:rdBuf,bindGroup:bg,inFlight:null});
      }
    }
    batchStepPoint = ecScalarMul(BigInt(BATCH_SIZE), {x:SECP_GX,y:SECP_GY});
    return true;
  }
  async function acquireGpuLease() {
    const lease = reserve('gpu', cfg.gpuKeys);
    if (!lease) return false;
    gpuLease = lease;
    currentSliceStart = lease.start;
    currentSliceEnd = lease.end + 1n;
    currentKeyVal = lease.start;
    currentBatchPoint = ecScalarMul(currentKeyVal, {x:SECP_GX,y:SECP_GY});
    keysCheckedSlice = 0;
    slicesVisited++;
    h('slicesVisitedCount').textContent = slicesVisited;
    renderLeases();
    return true;
  }
  async function gpuLoop(token) {
    if (!(await setupGpu()) || !isRunning || token !== hybridToken) return;
    let stageIndex = 0;
    if (!(await acquireGpuLease())) { log('Geen vrije GPU-range beschikbaar; CPU-WASM blijft actief.', 'warn'); return; }
    while (isRunning && token === hybridToken) {
      if (currentKeyVal >= currentSliceEnd) {
        for (const stage of pipelineStages) if (stage.inFlight) await stage.inFlight;
        release('gpu'); gpuLease = null;
        if (!(await acquireGpuLease())) break;
      }
      const actual = Math.min(BATCH_SIZE, Number(currentSliceEnd-currentKeyVal));
      const stage = pipelineStages[stageIndex]; stageIndex=(stageIndex+1)%PIPELINE_DEPTH;
      if (stage.inFlight) await stage.inFlight;
      if (!isRunning || token !== hybridToken) break;
      const batchKey=currentKeyVal, batchPoint=currentBatchPoint;
      currentKeyVal += BigInt(actual);
      if (actual===BATCH_SIZE) currentBatchPoint=ecAdd(batchPoint,batchStepPoint);
      preallocatedFullBuf.set(bigIntToLimbsLE(batchKey),0); preallocatedFullBuf[7]=actual;
      preallocatedFullBuf.set(bigIntToLimbsLE(batchPoint.x),8); preallocatedFullBuf.set(bigIntToLimbsLE(batchPoint.y),16);
      gpuDevice.queue.writeBuffer(stage.batchBuffer,0,preallocatedFullBuf); gpuDevice.queue.writeBuffer(stage.resBuf,0,zeroRes.buffer);
      const enc=gpuDevice.createCommandEncoder(),pass=enc.beginComputePass();
      pass.setPipeline(gpuPipeline);pass.setBindGroup(0,stage.bindGroup);pass.dispatchWorkgroups(Math.ceil(actual/256));pass.end();enc.copyBufferToBuffer(stage.resBuf,0,stage.readBuf,0,8);gpuDevice.queue.submit([enc.finish()]);
      stage.inFlight=(async()=>{await stage.readBuf.mapAsync(GPUMapMode.READ);const r=new Uint32Array(stage.readBuf.getMappedRange()),hit=r[0]===1,idx=r[1];stage.readBuf.unmap();if(isRunning&&token===hybridToken){gpuScanned+=actual;updateHybridCounters();}if(hit&&!matchFound)await reportMatch((batchKey+BigInt(idx)).toString(16).padStart(64,'0'),'WebGPU');})();
    }
  }
  startBtn.addEventListener('click', async () => {
    try {
      if (!validateInputs() || isRunning) return;
      cfg = readConfig();
      isRunning=true; matchFound=false; hybridToken++; const token=hybridToken;
      activeLeases.clear();cpuScanned=0;gpuScanned=0;cpuCompleted=0;slicesVisited=0;keysCheckedGlobal=0;keysCheckedSlice=0;startTime=performance.now();keysLimitGlobalForUi=Number(cfg.gpuKeys);
      h('limitDisplay').textContent=Number(cfg.gpuKeys).toLocaleString('nl-NL');h('startBtn').disabled=true;h('stopBtn').disabled=false;h('statusVal').textContent='Hybride GPU + WASM start';h('statusVal').className='value running';
      log(`Hybride start: GPU-leases=${cfg.gpuKeys.toString()}, CPU=${cfg.cpuCount} × ${cfg.cpuKeys.toString()} keys.`, 'hop');
      requestAnimationFrame(updateUI);
      for(let i=0;i<cfg.cpuCount;i++) startCpuWorker(i,token);
      h('cpuStatus').value=cfg.cpuCount?`${cfg.cpuCount} native WASM-worker(s) actief`:'CPU uitgeschakeld';
      gpuLoop(token);
    } catch (e) { stopHybrid('Invoerfout'); log('❌ '+e.message,'warn'); }
  });
  stopBtn.addEventListener('click', () => stopHybrid('Gestopt door gebruiker.'));
})();
