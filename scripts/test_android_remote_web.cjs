#!/usr/bin/env node
"use strict";
const assert=require("node:assert/strict");
const fs=require("node:fs");
const cp=require("node:child_process");
const path=require("node:path");
const base=path.join(__dirname,"..","android","app","src","main","assets","remote");
const map=require(path.join(base,"remote-map.js"));
const js=path.join(base,"app.js");
const html=fs.readFileSync(path.join(base,"index.html"),"utf8");
cp.execFileSync(process.execPath,["--check",js]);
cp.execFileSync(process.execPath,["--check",path.join(base,"remote-map.js")]);

function nearly(actual,expected,tolerance=1e-7){
 assert.ok(Math.abs(actual-expected)<tolerance,actual+" should approximate "+expected);
}
for(const location of [[0,0],[39.86,-105.0],[-23.55,-46.63],[45.1,9.1]]){
 for(const zoom of [2,9,15]){
  const point=map.project(location[0],location[1],zoom);
  const roundtrip=map.unproject(point.x,point.y,zoom);
  nearly(roundtrip.lat,location[0]);
  nearly(roundtrip.lon,location[1]);
 }
}
const small={north:39.805,south:39.8,east:-104.995,west:-105.0};
const large={north:55,south:20,east:15,west:-120};
assert.equal(map.validArea(small),true);
assert.equal(map.validArea({...small,north:39.79}),false);
assert.equal(map.validArea({...small,east:-105.1}),false);
assert.equal(map.validArea({...small,north:90}),false);
assert.equal(map.validArea({...small,west:170,east:-170}),false);
assert.ok(map.tileEstimate(small,12)>0);
assert.ok(map.tileEstimate(small,12)<=2400);
assert.ok(map.tileEstimate(large,15)>2400);
assert.equal(map.tileEstimate(small,16),Infinity);
assert.equal(map.tileEstimate({...small,west:175,east:-175},12),Infinity);
assert.ok(map.tileEstimate(small,13)>=map.tileEstimate(small,12));
const fake=Object.create(map.Selector.prototype);
fake.lat=0;fake.lon=0;fake.zoom=5;fake.size=()=>({w:800,h:400});
const a=fake.coordinate({x:250,y:130}),b=fake.coordinate({x:570,y:280});
assert.ok(a.lat>b.lat,"screen north must be latitude north");
assert.ok(a.lon<b.lon,"screen west must be longitude west");
assert.equal(map.validArea({north:a.lat,south:b.lat,west:a.lon,east:b.lon}),true);

for(const id of ["map-viewport","map-pan","map-draw","download-area",
 "download-zoom","map-selection-info","area-list","message-directory",
 "switch-mt","switch-mc","conversation-title"]){
 assert.ok(html.includes('id="'+id+'"'),"Missing browser element "+id);
}
for(const tab of ["chats","channels","nodes"]){
 assert.ok(html.includes('data-message-view="'+tab+'"'),"Missing "+tab+" navigation");
}
assert.ok(html.indexOf('src="/remote-map.js"')>=0);
assert.ok(!html.includes('name="north"')&&!html.includes('name="west"'),
 "Latitude/longitude inputs are not part of the visual area selection UX");
const app=fs.readFileSync(js,"utf8");
for(const expression of ["renderMessageDirectory()","function ensureMap()",
 "function updateMapSelection()","MeshCrosswayMap.tileEstimate",
 "changeMessageView(b.dataset.messageView)"]){
 assert.ok(app.includes(expression),"Missing "+expression);
}
assert.ok(!html.includes('src="https://'),"Do not run third-party scripts inside an authenticated browser");
console.log("Remote browser UI, chats/channels/nodes, geo projection, rectangle and quota tests passed.");
