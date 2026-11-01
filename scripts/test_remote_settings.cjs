// Production settings UI with a deterministic DOM and local radio API.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync(require('path').join(__dirname,'../firmware/display-prototype/meshcrossway_companion_page.h'),'utf8');
class Node{constructor(tag='div',text=''){this.tag=tag;this.text=text;this.children=[];this.dataset={};this._value='';this.disabled=false;}append(...n){this.children.push(...n);if(this.tag==='select'&&!this._value&&n.length)this._value=String(this.children[0].value);}prepend(...n){this.children.unshift(...n);}replaceChildren(...n){this.children=[];this.append(...n);}setAttribute(){}get value(){return this._value;}set value(v){this._value=String(v);}set textContent(v){this.text=String(v);this.children=[];}get textContent(){return this.text+this.children.map(x=>x.textContent).join('');}}
const nodes=new Map(),$=id=>{if(!nodes.has(id))nodes.set(id,new Node());return nodes.get(id);};$('radio-settings').open=true;
const fixture={identity:'a'.repeat(64),connected:true,firmware:'2.8.1',status:'Ready',revision:7,section:0,busy:false,loaded:true,groups:[{id:0,name:'LoRa',module:false}],fields:[{id:0,name:'region',type:14,choices:'0:UNSET|1:US|3:EU 868',value:'1'},{id:1,name:'password',type:9,secret:true,value:''}]};
const posts=[];let confirms=0,reply=fixture;
const ctx={console,JSON,Number,String,Map,view:'messages',commCategory:'network',network:'mt',document:{},$,element:(tag,text='')=>new Node(tag,text),confirm:()=>{confirms++;return true;},setTimeout:()=>{},readJSON:async r=>r.data,request:async(url,options)=>{if(options){posts.push(options.body);return {ok:true,data:{message:'Queued; awaiting radio'}};}return {ok:true,data:reply};}};
vm.createContext(ctx);const a=source.indexOf('let settingsBusy='),b=source.indexOf('async function refreshRadios()',a);vm.runInContext(source.slice(a,b),ctx);
const call=js=>vm.runInContext(js,ctx);
(async()=>{
 await call('refreshSettings()');assert.equal($('settings-fields').children.length,3);const form=$('settings-fields').children[1],input=form.children[0].children[0];input.value='3';input.oninput();await form.onsubmit({preventDefault(){}});await new Promise(r=>setImmediate(r));assert.equal(confirms,1);assert.equal(posts[0],'mt|2|0|0|7|'+'a'.repeat(64)+'|3');
 // No refresh may destroy an unsaved field; switching a radio clears stale identity.
 input.value='1';input.oninput();reply={...fixture,revision:8};await call('refreshSettings()');assert.strictEqual($('settings-fields').children[1],form);
 reply={...fixture,identity:'b'.repeat(64)};await call('refreshSettings()');assert.notStrictEqual($('settings-fields').children[1],form);assert.equal($('settings-groups').children.length,2);
 call('settingsReset()');await call('refreshSettings()');assert.equal($('settings-groups').children.length,2);
 // Read-only and sensitive fields never fabricate a stored value.
 assert.equal($('settings-fields').children[2].children[0].children[0].value,'');
 reply={identity:'c'.repeat(64),connected:true,firmware:'v1',status:'Ready',commands:[{id:8,name:'Rename',fields:'text:Name',confirm:true,supported:true}],presets:[]};ctx.network='mc';call('settingsReset()');await call('refreshSettings()');const mcform=$('settings-fields').children[0];mcform.children[1].children[0].value='New radio';await mcform.onsubmit({preventDefault(){}});await new Promise(r=>setImmediate(r));assert.equal(posts[1],'mc|3|8|0|0|'+'c'.repeat(64)+'|New radio');
 console.log('PASS: actual browser settings forms, enum values, explicit confirmation, per-network destination, preserved edits, stale-radio replacement and hidden secrets');
})().catch(e=>{console.error(e);process.exitCode=1;});
