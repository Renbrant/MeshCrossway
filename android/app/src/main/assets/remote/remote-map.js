"use strict";
/*
 * Tiny dependency-free slippy-map viewer for selecting MapLibre OFFLINE bounds.
 * The browser only requests the OSM raster tiles currently on screen as a LIVE
 * preview (normal interactive browsing). It NEVER preloads, downloads, scrapes,
 * or stores OSM raster tiles for offline use. Offline downloads run separately
 * on the phone through MapLibre/OpenFreeMap under that provider's limits.
 *
 * See https://operations.osmfoundation.org/policies/tiles/
 */
(function(root){
 const MAX_LAT=85.05112878, TILE=256;
 const clamp=(value,min,max)=>Math.max(min,Math.min(max,value));
 function wrap(lng){return ((lng+180)%360+360)%360-180;}
 function project(lat,lon,zoom){
  const world=TILE*Math.pow(2,zoom);
  const safe=clamp(lat,-MAX_LAT,MAX_LAT);
  const sin=Math.sin(safe*Math.PI/180);
  return {x:(lon+180)/360*world,
          y:(0.5-Math.log((1+sin)/(1-sin))/(4*Math.PI))*world};
 }
 function unproject(x,y,zoom){
  const world=TILE*Math.pow(2,zoom);
  return {lat:Math.atan(Math.sinh(Math.PI*(1-2*clamp(y,0,world)/world)))*180/Math.PI,
          lon:x/world*360-180};
 }
 function validArea(area){
  return !!area&&[area.north,area.south,area.east,area.west].every(Number.isFinite)&&
   area.north>area.south&&area.east>area.west&&
   area.north<=MAX_LAT&&area.south>=-MAX_LAT&&
   area.west>=-180&&area.east<=180&&area.east-area.west<=180;
 }
 function tileEstimate(area,maximumZoom){
  const zmax=Number(maximumZoom);
  if(!validArea(area)||!Number.isInteger(zmax)||zmax<1||zmax>15)return Infinity;
  let total=0;
  for(let z=0;z<=zmax;z++){
   const n=Math.pow(2,z);
   const x1=Math.floor((area.west+180)/360*n);
   const x2=Math.floor((area.east+180)/360*n);
   const tileY=lat=>Math.floor(project(lat,0,z).y/TILE);
   const y1=tileY(area.north),y2=tileY(area.south);
   total+=(Math.min(n-1,x2)-Math.max(0,x1)+1)*
           (Math.min(n-1,y2)-Math.max(0,y1)+1);
  }
  return Math.max(0,total);
 }
 class Selector {
  constructor(viewport,options){
   this.viewport=viewport;
   this.options=options||{};
   this.lat=0;this.lon=0; // Neutral global default; My location requires browser permission.
   this.zoom=3;this.mode="pan";this.selection=null;this.saved=[];
   this.tiles=new Map();this.pending=null;this.drag=null;
   this.host=document.createElement("div");this.host.className="map-tiles";
   this.overlays=document.createElement("div");this.overlays.className="map-overlays";
   this.viewport.append(this.host,this.overlays);
   this.viewport.addEventListener("pointerdown",e=>this.down(e));
   this.viewport.addEventListener("pointermove",e=>this.move(e));
   this.viewport.addEventListener("pointerup",e=>this.up(e));
   this.viewport.addEventListener("pointercancel",e=>this.cancel(e));
   this.viewport.addEventListener("wheel",e=>{
    e.preventDefault();this.setZoom(this.zoom+(e.deltaY<0?1:-1));
   },{passive:false});
   this.viewport.addEventListener("keydown",e=>{
    if(e.key==="+"||e.key==="="){e.preventDefault();this.setZoom(this.zoom+1);}
    if(e.key==="-"){e.preventDefault();this.setZoom(this.zoom-1);}
   });
   if(typeof ResizeObserver!=="undefined")new ResizeObserver(()=>this.paint()).observe(viewport);
   else window.addEventListener("resize",()=>this.paint());
   this.paint();
  }
  size(){return {w:this.viewport.clientWidth,h:this.viewport.clientHeight};}
  center(lat,lon,zoom){
   if(!Number.isFinite(lat)||!Number.isFinite(lon))return;
   this.lat=clamp(lat,-MAX_LAT,MAX_LAT);
   this.lon=wrap(lon);
   if(Number.isFinite(zoom))this.zoom=clamp(Math.round(zoom),2,18);
   this.paint();
  }
  setZoom(zoom){this.center(this.lat,this.lon,zoom);}
  setMode(mode){
   this.mode=mode==="draw"?"draw":"pan";
   this.viewport.classList.toggle("drawing",this.mode==="draw");
   if(this.options.onMode)this.options.onMode(this.mode);
  }
  setAreas(list){this.saved=Array.isArray(list)?list.slice(0,30):[];this.paintOverlay();}
  setSelection(bounds){
   this.selection=validArea(bounds)?{north:bounds.north,south:bounds.south,
                                   west:bounds.west,east:bounds.east}:null;
   this.paintOverlay();
   if(this.options.onSelect)this.options.onSelect(this.selection);
  }
  clearSelection(){this.setSelection(null);}
  focusBounds(bounds){
   if(!validArea(bounds))return;
   const {w,h}=this.size();
   if(w<10||h<10)return;
   let fit=3;
   for(let zoom=17;zoom>=2;zoom--){
    const topLeft=project(bounds.north,bounds.west,zoom);
    const bottomRight=project(bounds.south,bounds.east,zoom);
    if(bottomRight.x-topLeft.x<=w*.78&&bottomRight.y-topLeft.y<=h*.74){fit=zoom;break;}
   }
   const a=project(bounds.north,bounds.west,fit),b=project(bounds.south,bounds.east,fit);
   const center=unproject((a.x+b.x)/2,(a.y+b.y)/2,fit);
   this.center(center.lat,center.lon,fit);
   this.setSelection(bounds);
  }
  pointer(e){
   const bounds=this.viewport.getBoundingClientRect(),{w,h}=this.size();
   return {x:clamp(e.clientX-bounds.left,0,w),y:clamp(e.clientY-bounds.top,0,h)};
  }
  coordinate(point){
   const {w,h}=this.size(),center=project(this.lat,this.lon,this.zoom);
   return unproject(center.x+point.x-w/2,center.y+point.y-h/2,this.zoom);
  }
  down(e){
   if(e.button!==0&&e.pointerType!=="touch")return;
   e.preventDefault();
   const point=this.pointer(e);
   this.drag={id:e.pointerId,start:point,last:point,lat:this.lat,lon:this.lon};
   this.viewport.setPointerCapture(e.pointerId);
   if(this.mode==="draw")this.preview(point,point);
  }
  move(e){
   if(!this.drag||this.drag.id!==e.pointerId)return;
   e.preventDefault();
   const p=this.pointer(e),d=this.drag;
   d.last=p;
   if(this.mode==="draw"){this.preview(d.start,p);return;}
   const center=project(d.lat,d.lon,this.zoom);
   const newCenter=unproject(center.x-(p.x-d.start.x),center.y-(p.y-d.start.y),this.zoom);
   this.lat=clamp(newCenter.lat,-MAX_LAT,MAX_LAT);
   this.lon=wrap(newCenter.lon);
   this.paint();
  }
  preview(a,b){
   const box=this.ensurePreview();
   const left=Math.min(a.x,b.x),top=Math.min(a.y,b.y);
   box.style.left=left+"px";box.style.top=top+"px";
   box.style.width=Math.abs(a.x-b.x)+"px";box.style.height=Math.abs(a.y-b.y)+"px";
  }
  ensurePreview(){
   if(!this.pending){
    this.pending=document.createElement("div");
    this.pending.className="area-box draft";
    this.overlays.append(this.pending);
   }
   return this.pending;
  }
  up(e){
   if(!this.drag||this.drag.id!==e.pointerId)return;
   const d=this.drag,p=this.pointer(e);
   this.drag=null;this.dropPreview();
   if(this.mode==="draw"){
    if(Math.abs(p.x-d.start.x)<16||Math.abs(p.y-d.start.y)<16){
     if(this.options.onError)this.options.onError("Drag a larger rectangle to select an area.");
     return;
    }
    const a=this.coordinate(d.start),b=this.coordinate(p);
    const bounds={north:Math.max(a.lat,b.lat),south:Math.min(a.lat,b.lat),
                  west:Math.min(a.lon,b.lon),east:Math.max(a.lon,b.lon)};
    if(!validArea(bounds)){
     if(this.options.onError)this.options.onError("Selection crosses the date line or is too large. Zoom in and redraw.");
     return;
    }
    this.setSelection(bounds);
    this.setMode("pan");
   }
  }
  cancel(e){if(this.drag&&this.drag.id===e.pointerId){this.drag=null;this.dropPreview();}}
  dropPreview(){if(this.pending){this.pending.remove();this.pending=null;}}
  paint(){
   const {w,h}=this.size();
   if(w<64||h<64)return;
   const center=project(this.lat,this.lon,this.zoom);
   const left=center.x-w/2,top=center.y-h/2;
   const max=Math.pow(2,this.zoom);
   const xmin=Math.floor(left/TILE),xmax=Math.floor((left+w-1)/TILE);
   const ymin=Math.max(0,Math.floor(top/TILE));
   const ymax=Math.min(max-1,Math.floor((top+h-1)/TILE));
   const keep=new Set();
   let count=0;
   for(let y=ymin;y<=ymax;y++)for(let x=xmin;x<=xmax;x++){
    if(++count>100)break;
    const wrapped=((x%max)+max)%max;
    const key=this.zoom+"/"+wrapped+"/"+y;
    // At zoom 2+ and ordinary screen sizes, duplicate wrapped tiles are rare.
    const displayKey=key+"/"+x;
    keep.add(displayKey);
    let img=this.tiles.get(displayKey);
    if(!img){
     img=document.createElement("img");
     img.className="map-tile";img.alt="";
     img.draggable=false;
     img.referrerPolicy="strict-origin-when-cross-origin";
     img.src="https://tile.openstreetmap.org/"+key+".png";
     this.tiles.set(displayKey,img);this.host.append(img);
    }
    img.style.left=(x*TILE-left)+"px";
    img.style.top=(y*TILE-top)+"px";
   }
   for(const [key,img] of this.tiles){
    if(!keep.has(key)){img.remove();this.tiles.delete(key);}
   }
   this.paintOverlay();
   if(this.options.onView)this.options.onView({lat:this.lat,lon:this.lon,zoom:this.zoom});
  }
  paintOverlay(){
   // Recreate only inexpensive vector outline elements; map tile images persist.
   this.overlays.replaceChildren();
   const {w,h}=this.size();
   if(w<64||h<64)return;
   for(const area of this.saved)this.outline(area,"saved",area.name||"Saved area");
   if(this.selection)this.outline(this.selection,"selected","Selected for phone download");
   if(this.drag&&this.mode==="draw")this.preview(this.drag.start,this.drag.last);
  }
  outline(bounds,kind,label){
   if(!validArea(bounds))return;
   const {w,h}=this.size(),center=project(this.lat,this.lon,this.zoom);
   const world=TILE*Math.pow(2,this.zoom);
   const a=project(bounds.north,bounds.west,this.zoom);
   const b=project(bounds.south,bounds.east,this.zoom);
   let l=a.x-center.x;
   if(l<-world/2)l+=world;
   if(l>world/2)l-=world;
   const x=l+w/2,y=a.y-center.y+h/2;
   const box=document.createElement("div");
   box.className="area-box "+kind;
   box.title=label;box.setAttribute("aria-label",label);
   box.style.left=x+"px";box.style.top=y+"px";
   box.style.width=Math.max(0,b.x-a.x)+"px";
   box.style.height=Math.max(0,b.y-a.y)+"px";
   this.overlays.append(box);
  }
 }
 const publicApi={Selector,project,unproject,tileEstimate,validArea};
 if(typeof module!=="undefined"&&module.exports)module.exports=publicApi;
 if(root)root.MeshCrosswayMap=publicApi;
})(typeof window!=="undefined"?window:null);
