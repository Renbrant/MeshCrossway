#include "lvgl.h"
#include "meshcrossway_navigation.h"
#include <cassert>
#include <filesystem>
struct StopTick {};
static void step(){fake_delay_hook=[](unsigned){throw StopTick{};};try{mcxnav::worker(nullptr);}catch(StopTick&){}fake_delay_hook=nullptr;}
int main(int argc,char **argv){assert(argc==2);lv_init();std::filesystem::create_directories(argv[1]);assert(mcxnav::start());assert(mcxnav::store->load(argv[1]));
 mcxgnss::latest.valid=true;mcxgnss::latest.latitude=39.9;mcxgnss::latest.longitude=-104.9;mcxgnss::latest.position_ms=lv_tick_get();
 assert(mcxnav::submit(mcxnav::START));step();assert(mcxnav::store->mode==mcxnav::RECORDING&&mcxnav::store->point_count==1);
 lv_tick_inc(2500);mcxgnss::latest.position_ms=lv_tick_get();mcxgnss::latest.latitude+=.0001;step();assert(mcxnav::store->point_count==2);
 const auto captured=mcxgnss::latest.latitude;assert(mcxnav::submit(mcxnav::SAVE_POI,0,"At button press"));mcxgnss::latest.latitude+=.01;step();assert(mcxnav::store->poi_count==1&&mcxnav::store->pois[0].point.latitude==captured);
 assert(mcxnav::submit(mcxnav::PAUSE));step();assert(mcxnav::store->mode==mcxnav::PAUSED);
 fake_free_space=1;assert(mcxnav::submit(mcxnav::RESUME));step();assert(mcxnav::store->mode==mcxnav::PAUSED);
 fake_free_space=1024ULL*1024*1024;assert(mcxnav::submit(mcxnav::STOP));step();assert(mcxnav::store->mode==mcxnav::STOPPED&&mcxnav::store->track_count==1&&mcxnav::store->tracks[0].gpx);
 auto *s=new mcxnav::Snapshot;assert(mcxnav::snapshot(*s)&&s->poi_count==1&&s->track_count==1);delete s;
 mcxgnss::settings.enabled=0;assert(!mcxnav::submit(mcxnav::SAVE_POI,0,"stale"));
 for(unsigned i=0;i<4;++i){assert(mcxnav::submit(mcxnav::NONE));}
 assert(!mcxnav::submit(mcxnav::NONE));step();assert(mcxnav::count==3);
 puts("PASS: production navigation worker, bounded command queue, button-time POI fix, stale-fix rejection, SD-space pause, snapshot and GPX completion");
}
