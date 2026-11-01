namespace mcxnavhttp {
// PRODUCTION_HTTP_HELPERS
}
#include "remote_settings.h"
static httpd_req_t post(const std::string &body,const std::string &header="1"){httpd_req_t r;r.body=body;r.content_len=body.size();r.header=header;mcxsettingshttp::send(&r);return r;}
static httpd_req_t get(const char *net){httpd_req_t r;r.query=std::string("network=")+net;mcxsettingshttp::state(&r);return r;}
int main(){
 meshmc::client=new meshmc::Client();meshmt::client=new meshmt::Client();assert(mcxremotechat::init());memory_ok=false;assert(!mcxsettingshttp::init());memory_ok=true;assert(mcxsettingshttp::init());owner_extra=mcxsettingshttp::poll;
 auto &m=*meshmt::client;m.connected=m.initialized=m.history_ready=true;m.data.radio[1]=22;
 auto &c=*meshmc::client;c.attach(173);c.initialized=true;c.protocol=14;c.public_key[0]=33;
 auto r=get("mt");assert(r.status=="200 OK"&&r.response.find("Lora")!=r.response.npos&&r.response.find("Notification ringtone")!=r.response.npos);
 r=post("mt|1|5|0|0||","");assert(r.status=="403 Forbidden");r=post("mt|9|5|0|0||");assert(r.status=="400 Bad Request");r=post("mt|1|5|0|0||");assert(r.status=="202 Accepted"&&m.settings.stage==meshmt::Settings::READ_SEND);
 m.settings.stage=meshmt::Settings::LOADED;m.settings.loaded=true;meshmt::Writer w(m.settings.current);w.scalar(7,1);m.settings.revision=10;
 char identity[65];mcxremotechat::token("mt",m.data.radio,7,identity);const auto &section=meshmt::settings::sections[5];unsigned field=0;while(strcmp(section.fields[field].name,"region"))++field;
 std::string prefix="mt|2|5|"+std::to_string(field)+"|10|";
 r=post(prefix+std::string(64,'0')+"|3");assert(r.status=="409 Conflict");r=post(prefix+identity+"|3");assert(r.status=="202 Accepted"&&m.settings.stage==meshmt::Settings::FRESH_SEND);assert(!mcxsettingshttp::work->value[0]);
 r=get("mc");assert(r.response.find("Radio parameters")!=r.response.npos&&r.response.find("presets")!=r.response.npos);
 c.last_frame.len=1;c.last_frame.data[0]=14;strcpy(c.result,"VERY_PRIVATE_VALUE");r=get("mc");assert(r.response.find("VERY_PRIVATE_VALUE")==r.response.npos);
 mcxremotechat::token("mc",c.public_key,32,identity);r=post(std::string("mc|3|8|0|0|")+identity+"|New name");assert(r.status=="202 Accepted"&&c.qcount==1&&c.queue[0].frame.data[0]==8);
 c.protocol=7;r=post(std::string("mc|3|66|0|0|")+identity+"|get radio");assert(r.status=="409 Conflict"&&c.qcount==1);
 owner_poll=false;r=post("mt|1|5|0|0||");assert(r.status=="503 Service Unavailable"&&mcxsettingshttp::phase==0);owner_poll=true;
 r=get("bad");assert(r.status=="400 Bad Request");
 puts("PASS settings HTTP: bounded PSRAM, validation, request header, network identity, firmware gating, secret redaction, local-owner dispatch and timeout cancellation");
}
