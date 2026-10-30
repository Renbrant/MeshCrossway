#pragma once
// Pure presentation model: local known-node and unread counts, never a claim
// to know the size of an entire mesh. No networking, allocation or LVGL calls.
#include "meshcrossway_gnss_parser.h"
#include <stdio.h>
namespace mcxhome {
enum class Phase {OFF,STARTING,IDLE,SCANNING,CONNECTING,VERIFYING,PAIRING,READY,FAILED};
struct Network {unsigned nodes=0,unread=0;bool known=false,connected=false,initialized=false,history=false,fault=false;};
struct RadioCard {char state[40]={},nodes[40]={},unread[32]={};bool online=false,working=false;};
static RadioCard radio(const Network &n,unsigned protocol,Phase phase,unsigned target,bool pin=false){
  RadioCard c;const bool active=target==protocol;
  const char *s="Disconnected";
  if(phase==Phase::OFF)s="Bluetooth off";
  else if(phase==Phase::STARTING){s="Starting Bluetooth";c.working=true;}
  else if(phase==Phase::SCANNING){s="Searching for radio";c.working=true;}
  else if(active||!target){
    switch(phase){
      case Phase::CONNECTING:s=active?"Connecting...":"Identifying radio...";c.working=true;break;
      case Phase::VERIFYING:s=active?"Verifying radio...":"Identifying radio...";c.working=true;break;
      case Phase::PAIRING:s=pin?"Enter PIN on device":"Pairing...";c.working=true;break;
      case Phase::FAILED:s="Connection failed";break;
      case Phase::READY:
        if(!active)break;
        if(n.fault)s="Radio error";
        else if(!n.connected||!n.initialized){s="Synchronizing radio";c.working=true;}
        else if(!n.history){s="Restoring messages";c.working=true;}
        else {s="Connected";c.online=true;}
        break;
      default:break;
    }
  }
  snprintf(c.state,sizeof(c.state),"%s",s);
  if(n.known){snprintf(c.nodes,sizeof(c.nodes),"%u nodes%s",n.nodes,c.online?"":" (saved)");snprintf(c.unread,sizeof(c.unread),"%u unread",n.unread);}
  else {snprintf(c.nodes,sizeof(c.nodes),"Nodes: --");snprintf(c.unread,sizeof(c.unread),"Unread: --");}
  return c;
}
struct GpsCard {char state[40]={},accuracy[64]={};bool valid=false;};
static GpsCard gps(const mcxgnss::Fix &f,bool enabled,int error,bool imperial){
  GpsCard c;c.valid=enabled&&!error&&f.valid;
  const char *state=!enabled?"GPS disabled":error?"GPS unavailable":!f.frames?"Waiting for GPS":!f.valid?"Searching / no fix":"FIX GNSS";
  if(c.valid&&f.have_quality){
    if(f.quality==2)state="FIX differential";
    else if(f.quality==4)state="RTK FIX";
    else if(f.quality==5)state="RTK FLOAT";
    else if(f.quality==6)state="Estimated position";
    else if(f.quality==7||f.quality==8)state="Simulated position";
  }
  snprintf(c.state,sizeof(c.state),"%s",state);
  if(c.valid&&f.have_accuracy)snprintf(c.accuracy,sizeof(c.accuracy),"Est. %s%.1f %s",f.accuracy_source==2?"hRMS ":"accuracy ",f.horizontal_accuracy*(imperial?3.28084:1),imperial?"ft":"m");
  else snprintf(c.accuracy,sizeof(c.accuracy),"Accuracy unavailable");
  return c;
}
}
