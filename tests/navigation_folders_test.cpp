#include "meshcrossway_navigation_store.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <memory>
using namespace mcxnav;
static std::string bytes(const std::filesystem::path &p){std::ifstream in(p,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
int main(int argc,char **argv){assert(argc==2);const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);auto s=std::make_unique<Store>();assert(s->load(argv[1]));
 Sample point;point.latitude=39.9;point.longitude=-104.9;assert(s->start());const auto id=s->current;assert(s->sample(point,true,1000));assert(s->stop());const auto prefs=bytes(root/"preferences.bin");
 assert(s->track_folder(id)==0&&s->folders_ready);assert(!s->create_folder("../bad"));assert(!s->create_folder(""));assert(!s->create_folder("  "));assert(s->create_folder("Caminhadas & trilhas"));const auto first=s->folders.folders[0].id;assert(s->create_folder("Car trips"));const auto second=s->folders.folders[1].id;
 assert(!s->create_folder("Car trips"));assert(s->move_track(id,first)&&s->tracks[0].folder==first);assert(!s->move_track(id,999));assert(s->rename_folder(first,"Trilhas da família"));assert(!s->rename_folder(first,"Car trips"));assert(bytes(root/"preferences.bin")==prefs);
 s.reset();s=std::make_unique<Store>();assert(s->load(argv[1])&&s->track_folder(id)==first&&s->tracks[0].folder==first);assert(!strcmp(s->folder(first)->name,"Trilhas da família"));
 // A failed transaction preserves both RAM state and previously committed data.
 std::filesystem::create_directory(root/"folders.tmp");assert(!s->move_track(id,second)&&s->track_folder(id)==first);assert(!s->create_folder("Unsaved")&&s->folders.folders[2].id==0);std::filesystem::remove(root/"folders.tmp");
 assert(s->delete_folder(first)&&s->track_folder(id)==0&&s->tracks[0].folder==0);Track track;assert(s->scan_track(id,track)&&track.points==1);assert(s->move_track(id,second));
 assert(s->start());const auto current=s->current;assert(s->move_track(current,second));assert(s->stop());assert(s->track_folder(current)==second);s.reset();s=std::make_unique<Store>();assert(s->load(argv[1])&&s->track_folder(current)==second);
 std::filesystem::copy_file(root/"folders.bin",root/"folders.bin.bak",std::filesystem::copy_options::overwrite_existing);{std::ofstream f(root/"folders.bin");f<<"broken";}s.reset();s=std::make_unique<Store>();assert(s->load(argv[1])&&s->folders_ready&&s->track_folder(id)==second);
 const auto good=bytes(root/"folders.bin.bak");{std::ofstream f(root/"folders.bin.bak");f<<"broken too";}s.reset();s=std::make_unique<Store>();assert(s->load(argv[1])&&!s->folders_ready&&!s->create_folder("Do not overwrite corruption"));
 {std::ofstream f(root/"folders.bin",std::ios::binary);f.write(good.data(),good.size());}s.reset();s=std::make_unique<Store>();assert(s->load(argv[1])&&s->folders_ready&&s->track_folder(id)==second);
 for(unsigned i=1;i<MAX_FOLDERS;++i){char name[20];snprintf(name,sizeof(name),"Folder %u",i);assert(s->create_folder(name));}assert(!s->create_folder("Over capacity"));
 // Bounded geometry includes endpoints and actual segment IDs without GPX export.
 const unsigned route=900,total=2049;char file[32];snprintf(file,sizeof(file),"%08x.trk",route);FILE *f=fopen((root/file).c_str(),"wb");assert(f);Header h;memcpy(h.magic,"MCXTRK1",8);h.id=route;h.crc=checksum(&h,offsetof(Header,crc));assert(fwrite(&h,1,sizeof(h),f)==sizeof(h));
 for(unsigned i=0;i<total;++i){Sample p;p.latitude=39.9+i*.000001;p.longitude=179.95+i*.0001;if(p.longitude>180)p.longitude-=360;p.segment=i<1024?1:2;p.crc=checksum(&p,offsetof(Sample,crc));assert(fwrite(&p,1,sizeof(p),f)==sizeof(p));}fclose(f);
 auto g=std::make_unique<Geometry>();assert(read_geometry(argv[1],route,total,*g)&&g->count==GEOMETRY_POINTS&&g->total==total);assert(g->points[0].segment==1&&g->points[g->count-1].segment==2);assert(fabs(g->points[g->count-1].latitude-(39.9+(total-1)*.000001))<1e-8);
 assert(!read_geometry(argv[1],route,total+1,*g));snprintf(file,sizeof(file),"%08x.del",route);f=fopen((root/file).c_str(),"wb");fclose(f);assert(!read_geometry(argv[1],route,total,*g));
 puts("PASS folders: legacy tracks/presets retained, create/move/rename/delete, restart, failed transaction, backup/corruption protection, capacity, bounded segmented/dateline geometry");
}
