// UI integration test against a simulated frame API. No hardware is changed.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const crc32=bytes=>{let c=0xffffffff;for(const b of bytes){c^=b;for(let n=0;n<8;n++)c=(c>>>1)^((c&1)?0xedb88320:0);}return (c^0xffffffff)>>>0;};
(async()=>{
const browser=await chromium.launch({headless:true,executablePath:process.env.CHROME_PATH||(process.platform==='darwin'?'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome':undefined)});
try{
const page=await browser.newPage({viewport:{width:1100,height:1100}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
let images=new Map([0,1,2,3,4].map(i=>[i,fs.readFileSync(path.join(root,`test/fixtures/p${i}.cyd`))]));
const state={version:'0.2.0',count:5,playing:true,seconds:10,rotation:4,brightness:200,current:0,width:240,height:320,storage:true,free:1700000,images:[0,1,2,3,4]};
let failUpload=false,uploads=0,settingsWrites=0,networkWrites=0,networkPolls=0;
const network={configured:false,connected:false,state:'not_configured',ssid:'',ip:'',hostname:'cyd-photo-demo.local',ap_ssid:'CYD-Photo-DEMO',ap_ip:'192.168.4.1'};
await page.route('http://frame.test/**',async route=>{
const req=route.request(),url=new URL(req.url()),body=req.postDataBuffer(),params=new URLSearchParams(body?.toString()||'');
let response={status:200,body:''};
if(url.pathname==='/')response={status:200,contentType:'text/html; charset=utf-8',body:fs.readFileSync(path.join(root,'web/index.html'),'utf8')};
else if(url.pathname==='/api/status')response={status:200,contentType:'application/json',body:JSON.stringify(state)};
else if(url.pathname==='/api/image')response={status:200,contentType:'application/octet-stream',body:images.get(Number(url.searchParams.get('id')))};
else if(url.pathname==='/api/network'&&req.method()==='GET'){if(network.state==='connecting'&&++networkPolls>=2)Object.assign(network,{connected:true,state:'connected',ip:'192.0.2.10'});response={status:200,contentType:'application/json',body:JSON.stringify(network)};}
else if(req.headers()['x-photo-request']!=='1')response={status:403,body:'Missing header'};
else if(url.pathname==='/api/network'){networkWrites++;Object.assign(network,{configured:true,connected:false,state:'connecting',ssid:params.get('ssid'),ip:''});networkPolls=0;response={status:202,body:'Saved'};}
else if(url.pathname==='/api/network/forget')Object.assign(network,{configured:false,connected:false,state:'not_configured',ssid:'',ip:''});
else if(url.pathname==='/api/settings'){settingsWrites++;Object.assign(state,{seconds:Number(params.get('seconds')),rotation:Number(params.get('rotation')),brightness:Number(params.get('brightness')),playing:params.get('playing')==='1'});state.width=state.rotation%2?320:240;state.height=state.rotation%2?240:320;}
else if(url.pathname==='/api/control'){let i=state.images.indexOf(state.current);state.current=params.get('action')==='show'?Number(params.get('id')):state.images[(i+(params.get('action')==='next'?1:state.count-1))%state.count];}
else if(url.pathname==='/api/order')state.images=params.get('ids').split(',').map(Number);
else if(url.pathname==='/api/delete'){const id=Number(params.get('id'));images.delete(id);state.images=state.images.filter(x=>x!==id);state.count--;state.current=state.images[0]??-1;}
else if(url.pathname==='/api/upload'){
if(failUpload)response={status:400,body:'Simulated storage failure'};
else {const start=body.indexOf(Buffer.from('\r\n\r\n'))+4,raw=body.subarray(start,start+153616);assert.equal(raw.length,153616);assert.equal(raw.subarray(0,4).toString(),'CYD1');assert.equal(raw.readUInt32LE(12),crc32(raw.subarray(16)));let id=0;while(images.has(id))id++;images.set(id,Buffer.from(raw));state.images.push(id);state.count++;uploads++;}}
else response={status:404,body:'Unknown endpoint'};
await route.fulfill(response);
});
await page.goto('http://frame.test/');await page.waitForFunction(()=>document.getElementById('count').textContent==='5 / 12'&&!document.getElementById('save').disabled);
await page.screenshot({path:path.join(root,'docs/manager-desktop-demo.png'),fullPage:true});
await page.setViewportSize({width:390,height:844});
await page.reload();await page.waitForFunction(()=>document.getElementById('count').textContent==='5 / 12'&&!document.getElementById('save').disabled);
await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
await page.screenshot({path:path.join(root,'docs/manager-mobile-demo.png'),fullPage:false});
assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true,'Mobile has horizontal overflow');
await page.waitForFunction(()=>!document.getElementById('wifiSave').disabled);
await page.locator('#wifiSsid').fill('Home-2.4G');await page.locator('#wifiPassword').fill('short');await page.locator('#wifiSave').click();assert.equal(networkWrites,0);assert.match(await page.locator('#wifiMessage').textContent(),/8～63/);
await page.locator('#wifiPassword').fill('demo-pass');await page.locator('#wifiSave').click();await page.waitForFunction(()=>!document.getElementById('homeLink').hidden);assert.equal(networkWrites,1);assert.equal(network.ssid,'Home-2.4G');assert.equal(await page.locator('#wifiPassword').inputValue(),'');assert.equal(JSON.stringify(network).includes('demo-pass'),false);
await page.locator('#wifiSsid').fill('Unsaved edit');await page.evaluate(()=>refreshNetwork());assert.equal(await page.locator('#wifiSsid').inputValue(),'Unsaved edit');

const roundtrip=await page.evaluate(async()=>{const buffer=await(await fetch('/api/image?id=0')).arrayBuffer(),c=document.createElement('canvas');decodeCanvas(buffer,c);return [...new Uint8Array(encodeCanvas(c))];});
assert.deepEqual(Buffer.from(roundtrip),images.get(0),'RGB565 roundtrip differs');
assert.equal(await page.locator('#fit').inputValue(),'contain');
const fullImage=await page.evaluate(async()=>{
    const source=document.createElement('canvas');source.width=400;source.height=100;
    const c=source.getContext('2d');c.fillStyle='#ff0000';c.fillRect(0,0,400,100);c.fillStyle='#0000ff';c.fillRect(380,0,20,100);
    const file=new File([await new Promise(resolve=>source.toBlob(resolve))],'wide.png',{type:'image/png'});
    const out=document.createElement('canvas');await convert(file,out);const ctx=out.getContext('2d');
    const color=(x,y)=>[...ctx.getImageData(x,y,1,1).data].slice(0,3);
    return {width:out.width,height:out.height,top:color(0,0),left:color(1,120),right:color(318,120)};
});
assert.deepEqual(fullImage,{width:320,height:240,top:[0,0,0],left:[255,0,0],right:[0,0,255]},'Default full-image mode cropped an edge');
await page.locator('#next').click();await page.waitForFunction(()=>document.querySelector('.card.active span')?.textContent.includes('圖片 2'));
await page.locator('#seconds').fill('1');await page.locator('#save').click();assert.equal(settingsWrites,0);assert.match(await page.locator('#settingMessage').textContent(),/2～3600/);
await page.locator('#seconds').fill('3');await page.locator('#rotation').selectOption('1');await page.locator('#playing').uncheck();await page.locator('#save').click();await page.waitForFunction(()=>document.getElementById('settingMessage').textContent==='設定已儲存');assert.equal(state.rotation,1);assert.equal(state.playing,false);
await page.locator('.card').nth(0).getByRole('button',{name:'↓',exact:true}).click();await page.waitForFunction(()=>document.querySelector('.card span')?.textContent.includes('圖片 2'));assert.deepEqual(state.images,[1,0,2,3,4]);
page.on('dialog',d=>d.accept());await page.locator('.card').first().getByRole('button',{name:'刪除'}).click();await page.waitForFunction(()=>document.getElementById('count').textContent==='4 / 12');assert.equal(state.count,4);
await page.locator('#rotation').selectOption('4');await page.locator('#save').click();await page.waitForFunction(()=>!document.getElementById('save').disabled);assert.equal(state.rotation,4);
await page.locator('#files').setInputFiles([path.join(root,'test/fixtures/p0.png'),path.join(root,'test/fixtures/p3.png')]);await page.waitForFunction(()=>document.getElementById('message').textContent.includes('已選 2 張'));await page.locator('#upload').click();await page.waitForFunction(()=>document.getElementById('count').textContent==='6 / 12'&&!document.getElementById('save').disabled);assert.equal(uploads,2);assert.equal(images.get(1).readUInt16LE(4),240);assert.equal(images.get(1).readUInt16LE(6),320);assert.equal(images.get(5).readUInt16LE(4),320);assert.equal(images.get(5).readUInt16LE(6),240);
failUpload=true;await page.locator('#files').setInputFiles(path.join(root,'test/fixtures/p0.png'));await page.waitForFunction(()=>document.getElementById('message').textContent.includes('已選 1 張'));await page.locator('#upload').click();await page.waitForFunction(()=>document.getElementById('message').textContent.includes('上傳停止')&&!document.getElementById('save').disabled);assert.equal(state.count,6);
await page.locator('#wifiForget').click();await page.waitForFunction(()=>document.getElementById('wifiStatus').textContent.includes('尚未設定'));assert.equal(network.configured,false);
assert.deepEqual(errors,[]);console.log('PASS: desktop/mobile layout, RGB565 roundtrip, controls, settings validation, rotation, automatic mixed-orientation uploads, order, deletion, batch upload, checksum, Wi-Fi configuration/status/forget and upload failure recovery');
}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
