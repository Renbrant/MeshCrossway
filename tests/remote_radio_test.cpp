namespace meshble {
enum Phase{OFF,STARTING,IDLE,SCANNING,CONNECTING,VERIFYING,PAIRING,READY,FAILED};enum Radio{UNKNOWN,MESHTASTIC,MESHCORE};
struct Address{uint8_t type=0,val[6]={};};struct Peer{Address addr;char name[29]={};int rssi=-50;Radio advertised=UNKNOWN;};
struct Snapshot{Phase phase=OFF;Radio radio=UNKNOWN;Peer peers[8];unsigned count=0;char active_name[29]={},status[64]="Bluetooth off";bool pin_required=false,confirm_required=false;uint32_t compare_number=0;};
static Snapshot current;static int connected=-1;static uint32_t submitted=0;
static Snapshot snapshot(){return current;}
static bool request_start(){current.phase=STARTING;return true;}
static void scan(bool automatic){assert(!automatic);current.phase=SCANNING;}
static void connect(unsigned i){connected=int(i);current.phase=CONNECTING;}
static void connect_peer(const Peer &p){for(unsigned i=0;i<current.count;++i)if(!memcmp(current.peers[i].addr.val,p.addr.val,6)){connect(i);return;}}
static void disconnect(){current.phase=IDLE;}
static bool submit_pin(uint32_t n){submitted=n;current.pin_required=false;return true;}
static bool confirm_number(bool){current.confirm_required=false;return true;}
}
namespace mcxnavhttp {
// PRODUCTION_HTTP_HELPERS
}
#include "remote_radio.h"
static httpd_req_t radio_request(const std::string &body,const std::string &header="1"){
 httpd_req_t r;r.header=header;r.body=body;r.content_len=body.size();mcxremoteradio::send(&r);return r;
}
int main(){
 meshmc::client=new meshmc::Client();meshmt::client=new meshmt::Client();
 assert(mcxremotechat::init());owner_extra=mcxremoteradio::poll;
 auto r=radio_request("1||0","");assert(r.status=="403 Forbidden");
 r=radio_request("9||0");assert(r.status=="400 Bad Request");
 r=radio_request("1||0");assert(r.status=="202 Accepted"&&meshble::current.phase==meshble::STARTING);
 meshble::current.phase=meshble::IDLE;r=radio_request("2||0");assert(r.status=="202 Accepted"&&meshble::current.phase==meshble::SCANNING);
 auto &s=meshble::current;s.count=2;s.peers[0].addr.val[0]=1;s.peers[1].addr.val[0]=2;strcpy(s.peers[0].name,"My radio");strcpy(s.peers[1].name,"Other radio");
 char token[65];mcxremoteradio::peer_token(s.peers[0],token);std::swap(s.peers[0],s.peers[1]);r=radio_request(std::string("3|")+token+"|0");assert(r.status=="202 Accepted"&&meshble::connected==1); // Index changed, address identity did not.
 r=radio_request(std::string("3|")+token+"|0");assert(r.status=="409 Conflict");
 r=radio_request("4||0");assert(r.status=="202 Accepted"&&s.phase==meshble::IDLE);
 r=radio_request("5||123456");assert(r.status=="409 Conflict");s.pin_required=true;r=radio_request("5||123456");assert(r.status=="202 Accepted"&&meshble::submitted==123456&&mcxremoteradio::command.value==0);
 s.confirm_required=true;s.compare_number=123456;r=radio_request("6||1");assert(r.status=="202 Accepted"&&!s.confirm_required);
 r=radio_request("6||2");assert(r.status=="400 Bad Request");
 owner_poll=false;r=radio_request("1||0");assert(r.status=="503 Service Unavailable"&&mcxremoteradio::phase==0);owner_poll=true;
 httpd_req_t state;mcxremoteradio::state(&state);assert(state.response.find("My radio")!=state.response.npos&&state.response.find(token)!=state.response.npos);
 puts("PASS remote radio: action/header bounds, stale-index-safe identity, scan/connect/disconnect, PIN/confirmation state gates, secret clearing and cancelled timeout");
}
