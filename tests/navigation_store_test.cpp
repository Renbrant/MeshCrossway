#include "meshcrossway_navigation_store.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
using namespace mcxnav;
int main(int argc,char **argv){assert(argc==2);std::filesystem::create_directories(argv[1]);
 auto *s=new Store;assert(s->load(argv[1]));Sample p;p.latitude=39.9;p.longitude=-104.9;p.altitude=1500;p.flags=1;p.epoch=1793322000;
 assert(s->start());const auto id=s->current;assert(!s->sample(p,false,1000));assert(s->sample(p,true,2000));p.longitude+=.0001;assert(!s->sample(p,true,2100));assert(s->sample(p,true,4000));assert(s->point_count==2&&s->metres>0);
 assert(s->pause());p.longitude+=.5;assert(!s->sample(p,true,6000));assert(s->resume());assert(s->sample(p,true,8000));assert(s->metres<20);assert(s->stop());assert(s->track_count==1&&s->tracks[0].gpx);
 assert(s->save_poi(p,"Camp & <Rock>"));assert(s->poi_count==1);const auto poi=s->pois[0].id;
 delete s;s=new Store;assert(s->load(argv[1])&&s->poi_count==1&&s->track_count==1);assert(!strcmp(s->pois[0].name,"Camp & <Rock>"));
 std::ifstream in(std::string(argv[1])+"/pois.gpx");std::string xmltext((std::istreambuf_iterator<char>(in)),{});assert(xmltext.find("Camp &amp; &lt;Rock&gt;")!=std::string::npos);
 assert(s->start());const auto interrupted=s->current;assert(s->sample(p,true,10000));delete s;
 char path[256];snprintf(path,sizeof(path),"%s/%08x.trk",argv[1],interrupted);FILE *f=fopen(path,"ab");assert(f);fwrite("truncated",1,9,f);fclose(f);
 s=new Store;assert(s->load(argv[1])&&s->track_count==2);assert(s->tracks[0].id==interrupted&&s->tracks[0].points==1&&s->tracks[0].recovered&&s->tracks[0].gpx);Track t;assert(s->scan_track(id,t,true)&&t.points==3&&s->preview_count==3);assert(s->delete_poi(poi));
 delete s;s=new Store;assert(s->load(argv[1])&&s->poi_count==0);p.latitude=NAN;assert(!s->save_poi(p,"bad"));delete s;
 // Stationary recordings do not accumulate timed points, and saved overlays
 // remain available while a second track is recording.
 s=new Store;assert(s->load(argv[1]));p.latitude=39.9;p.longitude=-104.9;
 assert(s->preset(3,"Trail custom",4,2));assert(!s->preset(4,"bad",4,2));assert(!s->preset(3,"bad",0,2));
 assert(s->select_preset(3));assert(s->visible(id,true));assert(s->color(id,0x4DB8FF));
 assert(s->start());const auto active=s->current;assert(s->sample(p,true,20000,0));
 assert(!s->sample(p,true,120000,0));assert(s->point_count==1);
 p.longitude+=.0001;assert(!s->sample(p,true,122000,0.2));
 assert(s->sample(p,true,124000,5));assert(s->overlay_count>=1);
 bool earlier=false;for(unsigned i=0;i<s->overlay_count;++i)earlier|=s->overlays[i].id==id;assert(earlier);
 assert(s->visible(id,false));for(unsigned i=0;i<s->overlay_count;++i)assert(s->overlays[i].id!=id);
 assert(s->visible(id,true));assert(s->stop());delete s;
 s=new Store;assert(s->load(argv[1]));assert(s->preferences.selected==3);assert(!strcmp(s->preferences.presets[3].name,"Trail custom"));assert(s->style(id)->visible&&s->style(id)->color==0x4DB8FF);assert(s->style(active));delete s;
 s=new Store;assert(s->load(argv[1]));assert(s->rename_track(id,"River & Ridge"));Track named;assert(s->scan_track(id,named)&&!strcmp(named.name,"River & Ridge"));
 char named_path[256];snprintf(named_path,sizeof(named_path),"%s/%08x.gpx",argv[1],id);std::ifstream named_gpx(named_path);std::string named_xml((std::istreambuf_iterator<char>(named_gpx)),{});assert(named_xml.find("River &amp; Ridge")!=std::string::npos);
 const auto old_custom=s->preferences.presets[3].distance_m;assert(s->start_configured(4,0x123456,12,4));const auto fresh_id=s->current;assert(s->recording_preset.distance_m==12&&s->preferences.presets[3].distance_m==old_custom&&s->style(fresh_id)->color==0x123456);assert(!s->delete_track(fresh_id));assert(s->sample(p,true,200000,5));assert(s->rename_track(fresh_id,"New recording"));assert(s->stop());assert(s->delete_track(id));assert(!s->scan_track(id,named));delete s;
 s=new Store;assert(s->load(argv[1]));for(unsigned i=0;i<s->track_count;++i)assert(s->tracks[i].id!=id);assert(s->scan_track(fresh_id,named)&&!strcmp(named.name,"New recording"));assert(s->start());assert(s->current>fresh_id);assert(s->stop());delete s;
 puts("PASS: track start/pause/resume/segments, sample cadence, durable journal, reboot/torn-tail recovery, GPX and escaped POI persistence/delete");
}
