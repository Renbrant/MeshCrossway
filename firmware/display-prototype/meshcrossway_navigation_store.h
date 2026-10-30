#pragma once
// Original local navigation storage. No radio APIs, credentials or network I/O.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cerrno>
#include <ctime>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <initializer_list>
namespace mcxnav {
static constexpr unsigned MAX_POIS=64,MAX_TRACKS=32,MAX_PREVIEW=256;
struct Sample {double latitude=0,longitude=0,altitude=0;int64_t epoch=0;uint32_t segment=0,flags=0,crc=0,reserved=0;};
struct Poi {Sample point;uint32_t id=0;char name[40]={};uint32_t crc=0;};
struct Header {char magic[8]={};uint32_t id=0,crc=0;};
static_assert(sizeof(Sample)==48&&sizeof(Poi)==96&&sizeof(Header)==16,"Navigation schema layout");
struct Track {uint32_t id=0,points=0;double metres=0;Sample first;bool gpx=false,recovered=false;char name[40]={};};
static constexpr unsigned MAX_VISIBLE=8,OVERLAY_POINTS=128;
static constexpr uint32_t TRACK_COLORS[]={0xFF8A24,0x4DB8FF,0x72D8A5,0xFF7777,0xB99AFF,0xFFE066};
struct Preset {char name[24]="Walking";uint32_t distance_m=3,interval_s=2;};
struct Style {uint32_t id=0,color=0xFF8A24;bool visible=false;};
struct Overlay {uint32_t id=0,color=0xFF8A24;unsigned count=0;Sample points[OVERLAY_POINTS];};
struct Preferences {uint32_t magic=0x4d435832,selected=0;Preset presets[4];Style styles[MAX_TRACKS+1];uint32_t crc=0;};
enum Mode {STOPPED,RECORDING,PAUSED,STORAGE_ERROR};
enum Action {NONE,START,PAUSE,RESUME,STOP,SAVE_POI,DELETE_POI,SHOW_TRACK,SET_VISIBLE,SET_COLOR,SELECT_PRESET,SAVE_PRESET,EXPORT_TRACK,RENAME_TRACK,DELETE_TRACK,START_CONFIGURED};
struct Command {Action action=NONE;uint32_t id=0;Sample point;char name[40]={};uint32_t value=0,distance_m=0,interval_s=0;};
static uint32_t checksum(const void *p,size_t n){uint32_t c=2166136261u;auto *b=(const uint8_t *)p;for(size_t i=0;i<n;++i)c=(c^b[i])*16777619u;return c;}
static bool valid(const Sample &p){return std::isfinite(p.latitude)&&std::isfinite(p.longitude)&&p.latitude>=-90&&p.latitude<=90&&p.longitude>=-180&&p.longitude<=180&&(!(p.flags&1)||std::isfinite(p.altitude))&&!(p.flags&~1u)&&!p.reserved;}
static double distance(const Sample &a,const Sample &b){const double r=3.141592653589793/180,lat=(b.latitude-a.latitude)*r,lon=(b.longitude-a.longitude)*r;const double s=sin(lat/2)*sin(lat/2)+cos(a.latitude*r)*cos(b.latitude*r)*sin(lon/2)*sin(lon/2);return 12742000*asin(sqrt(fmin(1,fmax(0,s))));}
static bool same_sample(const Sample &p){return valid(p)&&p.crc==checksum(&p,offsetof(Sample,crc));}
static bool sync_file(FILE *f){return fflush(f)==0&&fsync(fileno(f))==0;}
// FAT implementations may refuse rename over an existing file. Keep the old
// export as a backup until the flushed replacement has its final name.
static bool commit_export(const char *tmp,const char *file){
 if(rename(tmp,file)==0)return true;
 struct stat st={};if(stat(file,&st))return false;
 char backup[208];snprintf(backup,sizeof(backup),"%s.bak",file);
 if(remove(backup)&&errno!=ENOENT)return false;
 if(rename(file,backup))return false;
 if(rename(tmp,file)){(void)rename(backup,file);return false;}
 (void)remove(backup);return true;
}
static void xml(FILE *f,const char *s){for(;*s;++s){switch(*s){case '&':fputs("&amp;",f);break;case '<':fputs("&lt;",f);break;case '>':fputs("&gt;",f);break;case '\"':fputs("&quot;",f);break;case '\'':fputs("&apos;",f);break;default:fputc(*s,f);}}}
struct Snapshot {Mode mode=STOPPED;uint32_t current=0,points=0,revision=0,preview_id=0;double metres=0;unsigned poi_count=0,track_count=0,preview_count=0;Preset recording_preset;char status[96]="Navigation storage starting";Poi pois[MAX_POIS];Track tracks[MAX_TRACKS];Sample preview[MAX_PREVIEW];Preferences preferences;Overlay overlays[MAX_VISIBLE];unsigned overlay_count=0;};
class Store {
public:
 char root[160]={},status[96]="SD navigation starting";
 Mode mode=STOPPED;uint32_t current=0,point_count=0,segment=1,revision=0;double metres=0;
 Poi pois[MAX_POIS];unsigned poi_count=0;Track tracks[MAX_TRACKS];unsigned track_count=0;
 Sample preview[MAX_PREVIEW];unsigned preview_count=0;uint32_t preview_id=0;
 Preferences preferences;Overlay overlays[MAX_VISIBLE];unsigned overlay_count=0;
 Preset recording_preset;char current_name[40]={};
 bool loaded=false;int error=0;
private:
 FILE *active=nullptr;Sample previous;bool have_previous=false,gap=true;uint32_t last_ms=0,next_track=1,next_poi=1;
 void path(char *out,size_t cap,uint32_t id,const char *suffix)const{snprintf(out,cap,"%s/%08lx.%s",root,(unsigned long)id,suffix);}
 void tell(const char *s){snprintf(status,sizeof(status),"%s",s);++revision;}
 bool failure(const char *s){error=errno?errno:EIO;if(active){fclose(active);active=nullptr;}mode=STORAGE_ERROR;char message[96];snprintf(message,sizeof(message),"%s (%d)",s,error);tell(message);return false;}
 bool deleted(uint32_t id)const{char file[192];path(file,sizeof(file),id,"del");FILE *f=fopen(file,"rb");if(!f)return false;Header h;bool ok=fread(&h,1,sizeof(h),f)==sizeof(h)&&!memcmp(h.magic,"MCXDEL1",8)&&h.id==id&&h.crc==checksum(&h,offsetof(Header,crc));fclose(f);return ok;}
 void track_name(uint32_t id,char *out,size_t cap)const{snprintf(out,cap,"Track %08lx",(unsigned long)id);for(const char *suffix:{"name","name.bak"}){char file[192];path(file,sizeof(file),id,suffix);FILE *f=fopen(file,"rb");if(!f)continue;struct Name{char text[40];uint32_t crc;} saved={};bool ok=fread(&saved,1,sizeof(saved),f)==sizeof(saved)&&fgetc(f)==EOF&&!ferror(f);fclose(f);if(ok&&memchr(saved.text,0,40)&&saved.text[0]&&saved.crc==checksum(saved.text,40)){snprintf(out,cap,"%s",saved.text);return;}}}
 bool header(FILE *f,uint32_t id){Header h;return fread(&h,1,sizeof(h),f)==sizeof(h)&&!memcmp(h.magic,"MCXTRK1",8)&&h.id==id&&h.crc==checksum(&h,offsetof(Header,crc));}
 void add_preview(const Sample &p){if(preview_count==MAX_PREVIEW){unsigned out=0;for(unsigned i=0;i<preview_count;i+=2)preview[out++]=preview[i];preview_count=out;}preview[preview_count++]=p;}
 bool point_xml(FILE *f,const Sample &p,const char *tag){fprintf(f,"<%s lat=\"%.8f\" lon=\"%.8f\">",tag,p.latitude,p.longitude);if(p.flags&1)fprintf(f,"<ele>%.2f</ele>",p.altitude);if(p.epoch>1577836800){time_t t=time_t(p.epoch);struct tm utc={};if(gmtime_r(&t,&utc)){char text[32];strftime(text,sizeof(text),"%Y-%m-%dT%H:%M:%SZ",&utc);fprintf(f,"<time>%s</time>",text);}}return !ferror(f);}
 bool poi_export(){char tmp[192],file[192];snprintf(tmp,sizeof(tmp),"%s/pois.gpx.tmp",root);snprintf(file,sizeof(file),"%s/pois.gpx",root);FILE *f=fopen(tmp,"wb");if(!f)return false;fputs("<?xml version=\"1.0\"?><gpx version=\"1.1\" creator=\"MeshCrossway\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n",f);for(unsigned i=0;i<poi_count;++i){point_xml(f,pois[i].point,"wpt");fputs("<name>",f);xml(f,pois[i].name);fputs("</name></wpt>\n",f);}fputs("</gpx>\n",f);bool ok=!ferror(f)&&sync_file(f);if(fclose(f))ok=false;if(ok)ok=commit_export(tmp,file);if(!ok)remove(tmp);return ok;}
public:
 Store(){const char *names[]={"Walking","Cycling","Car","Custom"};const unsigned distances[]={3,8,20,5},intervals[]={2,3,5,2};for(unsigned i=0;i<4;++i){snprintf(preferences.presets[i].name,24,"%s",names[i]);preferences.presets[i].distance_m=distances[i];preferences.presets[i].interval_s=intervals[i];}}
 Style *style(uint32_t id,bool create=false){for(auto &v:preferences.styles)if(v.id==id)return &v;if(create)for(auto &v:preferences.styles)if(!v.id){v.id=id;v.color=TRACK_COLORS[id%6];return &v;}return nullptr;}
 bool save_preferences(){if(!loaded)return false;char file[192],tmp[192];snprintf(file,sizeof(file),"%s/preferences.bin",root);snprintf(tmp,sizeof(tmp),"%s/preferences.tmp",root);preferences.crc=checksum(&preferences,offsetof(Preferences,crc));FILE *f=fopen(tmp,"wb");if(!f)return false;bool ok=fwrite(&preferences,1,sizeof(preferences),f)==sizeof(preferences)&&sync_file(f);if(fclose(f))ok=false;if(ok)ok=commit_export(tmp,file);return ok;}
 void load_preferences(){
  for(const char *suffix:{"preferences.bin","preferences.bin.bak"}){
   char file[192];snprintf(file,sizeof(file),"%s/%s",root,suffix);FILE *f=fopen(file,"rb");if(!f)continue;
   Preferences candidate;bool ok=fread(&candidate,1,sizeof(candidate),f)==sizeof(candidate)&&fgetc(f)==EOF&&!ferror(f);fclose(f);
   ok=ok&&candidate.magic==0x4d435832&&candidate.selected<4&&candidate.crc==checksum(&candidate,offsetof(Preferences,crc));
   for(auto &p:candidate.presets)ok=ok&&memchr(p.name,0,sizeof(p.name))&&p.name[0]&&p.distance_m>=3&&p.distance_m<=1000&&p.interval_s>=1&&p.interval_s<=300;
   if(ok){preferences=candidate;return;}
  }
 }
 void rebuild_overlays(){overlay_count=0;for(auto &v:preferences.styles){if(!v.id||!v.visible||(v.id==current&&(mode==RECORDING||mode==PAUSED))||overlay_count==MAX_VISIBLE)continue;char file[192];path(file,sizeof(file),v.id,"trk");FILE *f=fopen(file,"rb");if(!f)continue;if(!header(f,v.id)){fclose(f);continue;}auto &o=overlays[overlay_count];o.id=v.id;o.color=v.color;o.count=0;struct stat st={};if(fstat(fileno(f),&st)||st.st_size>16*1024*1024){fclose(f);continue;}const unsigned total=unsigned((st.st_size-sizeof(Header))/sizeof(Sample)),n=total<OVERLAY_POINTS?total:OVERLAY_POINTS;for(unsigned i=0;i<n;++i){const unsigned index=n>1?unsigned(uint64_t(i)*(total-1)/(n-1)):0;Sample point;if(fseek(f,long(sizeof(Header)+index*sizeof(Sample)),SEEK_SET)||fread(&point,1,sizeof(point),f)!=sizeof(point)||!same_sample(point))break;o.points[o.count++]=point;}fclose(f);++overlay_count;}}
 bool visible(uint32_t id,bool show){bool found=id==current;for(unsigned i=0;i<track_count;++i)found|=tracks[i].id==id;if(!found)return false;unsigned n=0;for(auto &v:preferences.styles)n+=v.visible&&v.id!=id;if(show&&n>=MAX_VISIBLE){tell("Map limit: hide a track first (8 visible)");return false;}auto *v=style(id,true);if(!v)return false;const auto old=*v;v->visible=show;if(!save_preferences()){*v=old;tell("Track visibility save failed");return false;}rebuild_overlays();tell("Track visibility saved");return true;}
 bool color(uint32_t id,uint32_t rgb){auto *v=style(id);if(!v||rgb>0xffffff)return false;const auto old=v->color;v->color=rgb;if(!save_preferences()){v->color=old;return false;}rebuild_overlays();tell("Track color saved");return true;}
 bool preset(unsigned slot,const char *name,uint32_t metres,uint32_t seconds){if(slot>=4||!name||!name[0]||strlen(name)>23||metres<3||metres>1000||seconds<1||seconds>300)return false;auto old=preferences.presets[slot];snprintf(preferences.presets[slot].name,24,"%s",name);preferences.presets[slot].distance_m=metres;preferences.presets[slot].interval_s=seconds;if(!save_preferences()){preferences.presets[slot]=old;return false;}tell("Recording preset saved");return true;}
 bool select_preset(unsigned slot){if(slot>=4)return false;auto old=preferences.selected;preferences.selected=slot;if(!save_preferences()){preferences.selected=old;return false;}if(mode==RECORDING||mode==PAUSED)recording_preset=preferences.presets[slot];tell("Recording preset selected");return true;}
 ~Store(){if(active)fclose(active);}
 bool rename_track(uint32_t id,const char *name){
  bool found=id==current;for(unsigned i=0;i<track_count;++i)found|=tracks[i].id==id;
  if(!id||!loaded||!found||deleted(id)||!name||!name[0]||strlen(name)>39){tell("Invalid track name (1-39 UTF-8 bytes)");return false;}
  for(const unsigned char *p=(const unsigned char *)name;*p;++p)if(*p<32||*p=='|'){tell("Track name contains unsupported characters");return false;}
  struct Name{char text[40];uint32_t crc;} saved={};snprintf(saved.text,40,"%s",name);saved.crc=checksum(saved.text,40);char file[192],tmp[192];path(file,sizeof(file),id,"name");path(tmp,sizeof(tmp),id,"name.tmp");FILE *f=fopen(tmp,"wb");if(!f){tell("Track name save failed");return false;}bool ok=fwrite(&saved,1,sizeof(saved),f)==sizeof(saved)&&sync_file(f);if(fclose(f))ok=false;if(ok)ok=commit_export(tmp,file);if(!ok){tell("Track name save failed");return false;}
  if(id==current)snprintf(current_name,sizeof(current_name),"%s",name);
  for(unsigned i=0;i<track_count;++i)if(tracks[i].id==id)snprintf(tracks[i].name,sizeof(tracks[i].name),"%s",name);
  const bool active_track=id==current&&(mode==RECORDING||mode==PAUSED);if(!active_track&&!export_track(id)){tell("Name saved / GPX update failed");return true;}tell("Track renamed");return true;
 }
 bool delete_track(uint32_t id){
  if(id==current&&(mode==RECORDING||mode==PAUSED)){tell("Finish recording before deleting this track");return false;}bool found=false;for(unsigned i=0;i<track_count;++i)found|=tracks[i].id==id;if(!found)return false;
  char file[192];path(file,sizeof(file),id,"del");Header marker;memcpy(marker.magic,"MCXDEL1",8);marker.id=id;marker.crc=checksum(&marker,offsetof(Header,crc));FILE *f=fopen(file,"wb");if(!f){tell("Track delete failed");return false;}bool ok=fwrite(&marker,1,sizeof(marker),f)==sizeof(marker)&&sync_file(f);if(fclose(f))ok=false;if(!ok){remove(file);tell("Track delete failed");return false;}
  bool cleaned=true;for(const char *suffix:{"gpx","gpx.tmp","gpx.bak","name","name.bak","name.tmp","trk"}){path(file,sizeof(file),id,suffix);if(remove(file)&&errno!=ENOENT)cleaned=false;}
  for(unsigned i=0;i<track_count;++i)if(tracks[i].id==id){for(unsigned j=i+1;j<track_count;++j)tracks[j-1]=tracks[j];--track_count;break;}
  if(auto *v=style(id))*v=Style{};
  if(preview_id==id){preview_count=0;preview_id=0;}if(current==id){current=0;point_count=0;metres=0;current_name[0]=0;}save_preferences();rebuild_overlays();tell(cleaned?"Track deleted":"Track deleted / file cleanup pending");return true;
 }
 bool start_configured(unsigned slot,uint32_t color,uint32_t metres,uint32_t seconds){
  if(slot>4||color>0xffffff||(slot==4&&(metres<3||metres>1000||seconds<1||seconds>300))||mode==RECORDING||mode==PAUSED)return false;
  if(slot<4&&!select_preset(slot))return false;
  if(!start(color))return false;
  if(slot==4){snprintf(recording_preset.name,24,"Custom");recording_preset.distance_m=metres;recording_preset.interval_s=seconds;}return true;
 }
 bool scan_track(uint32_t id,Track &out,bool draw=false){char file[192];path(file,sizeof(file),id,"trk");if(deleted(id))return false;FILE *f=fopen(file,"rb");if(!f)return false;struct stat bounded={};if(fstat(fileno(f),&bounded)||bounded.st_size>16*1024*1024){fclose(f);return false;}if(!header(f,id)){fclose(f);return false;}out=Track{};out.id=id;track_name(id,out.name,sizeof(out.name));Sample p,last;bool have=false;if(draw){preview_count=0;preview_id=id;}
  for(;;){const size_t n=fread(&p,1,sizeof(p),f);if(!n)break;if(n!=sizeof(p)||!same_sample(p)){out.recovered=true;break;}if(!out.points)out.first=p;if(have&&last.segment==p.segment)out.metres+=distance(last,p);last=p;have=true;++out.points;if(draw)add_preview(p);}
  const bool ok=!ferror(f);fclose(f);path(file,sizeof(file),id,"gpx");struct stat st={};out.gpx=stat(file,&st)==0;return ok;
 }
 bool export_track(uint32_t id){if(deleted(id))return false;char input[192],tmp[192],file[192];path(input,sizeof(input),id,"trk");path(tmp,sizeof(tmp),id,"gpx.tmp");path(file,sizeof(file),id,"gpx");FILE *in=fopen(input,"rb");if(!in)return false;struct stat bounded={};if(fstat(fileno(in),&bounded)||bounded.st_size>16*1024*1024){fclose(in);return false;}if(!header(in,id)){fclose(in);return false;}FILE *f=fopen(tmp,"wb");if(!f){fclose(in);return false;}
  char name[40];track_name(id,name,sizeof(name));fputs("<?xml version=\"1.0\"?><gpx version=\"1.1\" creator=\"MeshCrossway\" xmlns=\"http://www.topografix.com/GPX/1/1\"><trk><name>",f);xml(f,name);fputs("</name>\n",f);Sample p;uint32_t last=0;bool open=false;
  while(fread(&p,1,sizeof(p),in)==sizeof(p)){if(!same_sample(p))break;if(!open||p.segment!=last){if(open)fputs("</trkseg>\n",f);fputs("<trkseg>\n",f);open=true;}last=p.segment;point_xml(f,p,"trkpt");fputs("</trkpt>\n",f);}
  const bool input_ok=!ferror(in);fclose(in);if(open)fputs("</trkseg>",f);fputs("</trk></gpx>\n",f);bool ok=input_ok&&!ferror(f)&&sync_file(f);if(fclose(f))ok=false;if(ok)ok=commit_export(tmp,file);if(!ok)remove(tmp);return ok;
 }
 bool load(const char *directory){if(active)return false;if(strlen(directory)>=sizeof(root))return false;if(directory!=root)snprintf(root,sizeof(root),"%s",directory);if(mkdir(root,0755)&&errno!=EEXIST){tell("Navigation SD directory unavailable");return false;}DIR *dir=opendir(root);if(!dir){tell("Cannot read navigation SD");return false;}poi_count=track_count=0;next_track=next_poi=1;errno=0;
  while(auto *e=readdir(dir)){unsigned long id=0;char suffix[8]={},extra=0;if(sscanf(e->d_name,"%8lx.%7s%c",&id,suffix,&extra)!=2||!id||id>0xfffffffeUL)continue;char canonical[32];snprintf(canonical,sizeof(canonical),"%08lx.%s",id,suffix);if(strcmp(canonical,e->d_name))continue;
   if(!strcmp(suffix,"del")&&id>=next_track)next_track=uint32_t(id)+1;
   if(!strcmp(suffix,"trk")){if(id>=next_track)next_track=uint32_t(id)+1;unsigned slot=track_count;if(track_count==MAX_TRACKS){slot=0;for(unsigned i=1;i<track_count;++i)if(tracks[i].id<tracks[slot].id)slot=i;if(id<=tracks[slot].id)continue;}Track t;if(scan_track(uint32_t(id),t)){if(!t.gpx){t.recovered=true;t.gpx=export_track(uint32_t(id));}tracks[slot]=t;if(track_count<MAX_TRACKS)++track_count;}}
   if(!strcmp(suffix,"poi")){if(id>=next_poi)next_poi=uint32_t(id)+1;if(poi_count>=MAX_POIS)continue;char file[192];path(file,sizeof(file),uint32_t(id),"poi");FILE *f=fopen(file,"rb");Poi p;if(f){const bool ok=fread(&p,1,sizeof(p),f)==sizeof(p)&&fgetc(f)==EOF&&!ferror(f);fclose(f);if(ok&&p.id==id&&memchr(p.name,0,sizeof(p.name))&&valid(p.point)&&p.crc==checksum(&p,offsetof(Poi,crc)))pois[poi_count++]=p;}}
  }
  closedir(dir);for(unsigned i=0;i<track_count;++i)for(unsigned j=i+1;j<track_count;++j)if(tracks[j].id>tracks[i].id){const auto t=tracks[i];tracks[i]=tracks[j];tracks[j]=t;}
  loaded=true;mode=STOPPED;error=0;load_preferences();for(auto &v:preferences.styles){bool found=false;for(unsigned i=0;i<track_count;++i)found|=tracks[i].id==v.id;if(!found)v=Style{};}for(unsigned i=0;i<track_count;++i)style(tracks[i].id,true);rebuild_overlays();tell(poi_export()?"Navigation SD ready":"Navigation loaded / POI GPX export unavailable");return true;
 }
 bool start(uint32_t chosen_color=0xffffffffu){if(!loaded||mode==RECORDING||mode==PAUSED)return false;char file[192];uint32_t id=next_track;int fd=-1;for(unsigned n=0;n<128&&id&&id<0xffffffffu;++n,++id){path(file,sizeof(file),id,"trk");fd=open(file,O_WRONLY|O_CREAT|O_EXCL,0644);if(fd>=0)break;if(errno!=EEXIST)return failure("Cannot create track on SD");}if(fd<0)return failure("Track ID space unavailable");close(fd);active=fopen(file,"r+b");if(!active){const int saved=errno;remove(file);errno=saved;return failure("Track stream open failed");}Header h;memcpy(h.magic,"MCXTRK1",8);h.id=id;h.crc=checksum(&h,offsetof(Header,crc));if(fwrite(&h,1,sizeof(h),active)!=sizeof(h)||!sync_file(active))return failure("Track header save failed");next_track=id+1;current=id;point_count=0;metres=0;segment=1;gap=true;have_previous=false;preview_count=0;preview_id=id;mode=RECORDING;recording_preset=preferences.presets[preferences.selected];track_name(id,current_name,sizeof(current_name));auto *display=style(id,true);if(display&&chosen_color<=0xffffff)display->color=chosen_color;visible(id,true);rebuild_overlays();tell("Recording / waiting for GPS");return true;}
 bool sample(Sample p,bool fresh,uint32_t now,double speed_kmh=-1){if(mode!=RECORDING)return false;if(!fresh||!valid(p)){gap=true;tell("Recording / waiting for GPS");return false;}const auto &preset=recording_preset;
  if(have_previous&&uint32_t(now-last_ms)<preset.interval_s*1000)return false;
  // Never force a point merely because time elapsed. Reject low-speed drift;
  // real slow movement is retained once displacement exceeds the preset spacing.
  if(have_previous&&!gap&&(distance(previous,p)<preset.distance_m||(speed_kmh>=0&&speed_kmh<0.8)))return false;
  if(gap&&have_previous)++segment;
  p.segment=segment;p.reserved=0;p.crc=checksum(&p,offsetof(Sample,crc));
  struct stat st={};if(fstat(fileno(active),&st)||st.st_size+int(sizeof(Sample))>16*1024*1024)return failure("Track limit reached / saved prefix retained");
  if(fwrite(&p,1,sizeof(p),active)!=sizeof(p)||!sync_file(active))return failure("Track write failed / saved prefix retained");
  if(have_previous&&!gap)metres+=distance(previous,p);
  previous=p;have_previous=true;gap=false;last_ms=now;++point_count;add_preview(p);tell("Recording / point saved");return true;
 }
 bool pause(){if(mode!=RECORDING)return false;mode=PAUSED;gap=true;tell("Recording paused / saved");return true;}
 bool resume(){if(mode!=PAUSED)return false;mode=RECORDING;gap=true;tell("Recording resumed / waiting for GPS");return true;}
 bool stop(){if(mode!=RECORDING&&mode!=PAUSED)return false;bool ok=sync_file(active);if(fclose(active))ok=false;active=nullptr;mode=STOPPED;if(!ok)return failure("Track close failed / prefix retained");const bool exported=export_track(current);const auto id=current;load(root);Track t;if(scan_track(id,t,true)){point_count=t.points;metres=t.metres;}tell(exported?"Track and GPX saved":"Track saved / GPX export failed");return exported;}
 bool save_poi(const Sample &point,const char *name){if(!loaded||!valid(point)||poi_count>=MAX_POIS){tell("POI unavailable / invalid fix or list full");return false;}if(name&&strlen(name)>=sizeof(Poi::name)){tell("POI name is too long in UTF-8 bytes");return false;}Poi p;p.point=point;p.id=next_poi;if(!p.id||p.id==0xffffffffu)return false;snprintf(p.name,sizeof(p.name),"%s",name&&*name?name:"POI");for(char *c=p.name;*c;++c)if(uint8_t(*c)<32)*c=' ';p.crc=checksum(&p,offsetof(Poi,crc));char file[192];path(file,sizeof(file),p.id,"poi");const int fd=open(file,O_WRONLY|O_CREAT|O_EXCL,0644);if(fd<0){tell("POI ID conflict / restart to refresh");return false;}close(fd);FILE *f=fopen(file,"r+b");if(!f){const int saved=errno;remove(file);error=saved;char message[96];snprintf(message,sizeof(message),"POI open failed (%d)",saved);tell(message);return false;}bool ok=fwrite(&p,1,sizeof(p),f)==sizeof(p)&&sync_file(f);if(fclose(f))ok=false;if(!ok){remove(file);tell("POI save failed");return false;}pois[poi_count++]=p;++next_poi;tell(poi_export()?"POI saved / GPX updated":"POI saved / GPX update failed");return true;}
 bool delete_poi(uint32_t id){for(unsigned i=0;i<poi_count;++i)if(pois[i].id==id){char file[192];path(file,sizeof(file),id,"poi");if(remove(file)){tell("POI delete failed");return false;}for(unsigned j=i+1;j<poi_count;++j)pois[j-1]=pois[j];--poi_count;tell(poi_export()?"POI deleted / GPX updated":"POI deleted / GPX update failed");return true;}return false;}
};
}
