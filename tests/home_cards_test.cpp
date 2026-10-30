#include "lvgl.h"
#include "meshcrossway_home_status.h"
#include "meshcrossway_conversations.h"
#include "meshcrossway_meshtastic_protocol.h"
#include "meshcrossway_gnss_settings.h"
#include <atomic>
#include <cassert>
#include <cstring>
#include <memory>
#include <filesystem>
namespace meshble {
enum Phase{OFF,STARTING,IDLE,SCANNING,CONNECTING,VERIFYING,PAIRING,READY,FAILED};enum Radio{UNKNOWN,MESHTASTIC,MESHCORE};
struct Peer{Radio advertised=UNKNOWN;};struct Snapshot{Phase phase=OFF;Radio radio=UNKNOWN;int selected=-1,count=0;Peer peers[8];bool pin_required=false,confirm_required=false;};
static Snapshot current;static Snapshot snapshot(){return current;}
}
namespace meshmc {static Client *client=nullptr;static std::atomic<bool> link{false};}
namespace meshmt {static Client *client=nullptr;static std::atomic<bool> link{false};}
namespace meshnetwork {struct HomeStatus{char ssid[33]={},state[30]={};bool connected=false;};static HomeStatus wifi;static HomeStatus home_status(){return wifi;}}
namespace mcxgnss {static Fix fix;static Settings settings;struct Diagnostics{int error=0;};static Diagnostics diagnostics;static Fix snapshot(){return fix;}static Settings configuration(){return settings;}static Diagnostics status(){return diagnostics;}}
namespace meshcrossway {
static int page=0;static lv_obj_t *body=nullptr;
static const lv_color_t bg=lv_color_hex(0x090C10),panel=lv_color_hex(0x181D24),white=lv_color_hex(0xF2F4F7),muted=lv_color_hex(0x9AA6B4),orange=lv_color_hex(0xFF8A24),green=lv_color_hex(0x72D8A5),purple=lv_color_hex(0x9C88FF),cyan=lv_color_hex(0x6CBDE6);
static void fill(lv_obj_t *o,lv_color_t c,int radius=0){lv_obj_set_style_bg_color(o,c,0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_pad_all(o,0,0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_radius(o,radius,0);}
static lv_obj_t *panel_at(lv_obj_t *p,int x,int y,int w,int h,lv_color_t c,int r=0){auto *o=lv_obj_create(p);fill(o,c,r);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;}
static lv_obj_t *single_label(lv_obj_t *p,const char *s,int x,int y,int w,lv_color_t c){auto *o=lv_label_create(p);lv_label_set_text(o,s);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,19);lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);lv_obj_set_style_text_color(o,c,0);return o;}
namespace chatview {static void label(lv_obj_t *o,const char *s){if(o&&strcmp(lv_label_get_text(o),s))lv_label_set_text(o,s);}}
// PRODUCTION_HOME
}
static unsigned flushes=0;static lv_color_t pixels[480*320],buffer[480*20];
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){++flushes;for(int y=a->y1;y<=a->y2;++y)for(int x=a->x1;x<=a->x2;++x)pixels[y*480+x]=*p++;lv_disp_flush_ready(d);}
static void render(const char *name){const char *dir=getenv("MCX_RENDER_DIR");if(!dir)return;std::filesystem::create_directories(dir);lv_refr_now(nullptr);auto file=std::filesystem::path(dir)/(std::string(name)+".ppm");auto *f=fopen(file.c_str(),"wb");assert(f);fprintf(f,"P6\n480 320\n255\n");for(auto p:pixels){lv_color32_t c;c.full=lv_color_to32(p);unsigned char rgb[]={c.ch.red,c.ch.green,c.ch.blue};fwrite(rgb,1,3,f);}fclose(f);}
int main(){
 lv_init();lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,buffer,nullptr,480*20);lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=480;driver.ver_res=320;driver.draw_buf=&buf;driver.flush_cb=flush;lv_disp_drv_register(&driver);
 using namespace meshcrossway;fill(lv_scr_act(),bg);body=panel_at(lv_scr_act(),79,34,401,286,bg);
 auto cp=std::make_unique<meshmc::Client>();auto mp=std::make_unique<meshmt::Client>();meshmc::client=cp.get();meshmt::client=mp.get();
 auto &c=*cp;auto &m=*mp;c.initialized=c.connected=c.history_ready=true;c.contact_count=23;c.message_count=3;meshmc::link=true;
 c.messages[0].id=1;c.messages[0].read=false;c.messages[1].id=2;c.messages[1].outgoing=true;c.messages[2].id=3;c.messages[2].read=true;
 m.data.node_count=8;m.data.message_count=2;m.data.messages[0].seq=1;m.data.messages[1].seq=2;m.data.messages[1].outgoing=true;
 meshble::current.phase=meshble::READY;meshble::current.radio=meshble::MESHCORE;
 strcpy(meshnetwork::wifi.ssid,"My home Wi-Fi network name");strcpy(meshnetwork::wifi.state,"Connected to Wi-Fi");meshnetwork::wifi.connected=true;
 mcxgnss::fix.valid=true;mcxgnss::fix.frames=10;mcxgnss::fix.have_accuracy=true;mcxgnss::fix.horizontal_accuracy=2.1;mcxgnss::fix.accuracy_source=1;
 home_cards(401);lv_obj_update_layout(body);lv_refr_now(nullptr);
 assert(!strcmp(lv_label_get_text(home_mesh[1][0]),"Connected"));assert(!strcmp(lv_label_get_text(home_mesh[1][1]),"23 nodes"));assert(!strcmp(lv_label_get_text(home_mesh[1][2]),"1 unread"));
 assert(!strcmp(lv_label_get_text(home_mesh[0][0]),"Disconnected"));assert(!strcmp(lv_label_get_text(home_mesh[0][1]),"8 nodes (saved)"));assert(!strcmp(lv_label_get_text(home_mesh[0][2]),"1 unread"));
 assert(!strcmp(lv_label_get_text(home_wifi_name),meshnetwork::wifi.ssid));assert(!strcmp(lv_label_get_text(home_gps_state),"FIX GNSS"));assert(!strcmp(lv_label_get_text(home_gps_accuracy),"Est. accuracy 2.1 m"));
 render("home-live");
 memset(meshnetwork::wifi.ssid,'W',32);meshnetwork::wifi.ssid[32]=0;home_refresh(nullptr);lv_refr_now(nullptr);render("home-long-ssid");
 const auto idle_flushes=flushes;for(unsigned i=0;i<4;++i){home_refresh(nullptr);lv_refr_now(nullptr);}assert(flushes==idle_flushes); // No idle repaint or rebuild.
 c.messages[0].read=true;home_refresh(nullptr);assert(!strcmp(lv_label_get_text(home_mesh[1][2]),"0 unread"));
 meshble::current={};meshble::current.phase=meshble::CONNECTING;meshble::current.count=1;meshble::current.selected=0;meshble::current.peers[0].advertised=meshble::MESHTASTIC;
 home_refresh(nullptr);assert(!strcmp(lv_label_get_text(home_mesh[0][0]),"Connecting..."));assert(!strcmp(lv_label_get_text(home_mesh[1][0]),"Disconnected"));
 render("home-connecting");
 // After peer discovery, native synchronizing/history phases are still visible.
 meshble::current.radio=meshble::MESHTASTIC;meshble::current.phase=meshble::READY;meshmt::link=true;m.connected=true;home_refresh(nullptr);assert(!strcmp(lv_label_get_text(home_mesh[0][0]),"Synchronizing radio"));
 m.initialized=m.history_ready=true;home_refresh(nullptr);assert(!strcmp(lv_label_get_text(home_mesh[0][0]),"Connected"));
 mcxgnss::fix.valid=false;strcpy(meshnetwork::wifi.state,"Reconnecting...");meshnetwork::wifi.connected=false;home_refresh(nullptr);assert(!strcmp(lv_label_get_text(home_gps_accuracy),"Accuracy unavailable"));
 for(unsigned i=0;i<2;++i)for(auto *label:home_mesh[i]){lv_area_t box,parent;lv_obj_get_coords(label,&box);lv_obj_get_coords(lv_obj_get_parent(label),&parent);assert(box.y1>=parent.y1&&box.y2<=parent.y2&&box.x2<=parent.x2);}
 // Recreate with the left rail compact, then safely leave the page.
 home_clear();lv_obj_clean(body);lv_obj_set_width(body,440);home_cards(440);lv_obj_update_layout(body);render("home-wide");
 home_clear();lv_obj_clean(body);page=4;home_refresh(nullptr);
 puts("PASS Home real LVGL: live MC/MT nodes and unread, selected BLE connect/sync state, Wi-Fi name, GPS accuracy, widths, safe page changes and zero idle flushes");
}
