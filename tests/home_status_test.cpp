#include "meshcrossway_home_status.h"
#include <cassert>
#include <cstring>
using namespace mcxhome;
int main(){
 Network n;auto c=radio(n,1,Phase::OFF,0);assert(!strcmp(c.state,"Bluetooth off")&&!strcmp(c.nodes,"Nodes: --"));
 c=radio(n,1,Phase::CONNECTING,1);assert(c.working&&!strcmp(c.state,"Connecting..."));
 c=radio(n,2,Phase::CONNECTING,1);assert(!c.working&&!strcmp(c.state,"Disconnected"));
 c=radio(n,1,Phase::VERIFYING,0);assert(c.working&&!strcmp(c.state,"Identifying radio..."));
 c=radio(n,1,Phase::PAIRING,1,true);assert(!strcmp(c.state,"Enter PIN on device"));
 n.known=true;n.nodes=42;n.unread=5;n.connected=true;
 c=radio(n,2,Phase::READY,2);assert(!c.online&&!strcmp(c.state,"Synchronizing radio"));
 n.initialized=true;c=radio(n,2,Phase::READY,2);assert(!strcmp(c.state,"Restoring messages"));
 n.history=true;c=radio(n,2,Phase::READY,2);assert(c.online&&!strcmp(c.nodes,"42 nodes")&&!strcmp(c.unread,"5 unread"));
 c=radio(n,1,Phase::READY,2);assert(!c.online&&!strcmp(c.nodes,"42 nodes (saved)"));
 n.fault=true;c=radio(n,2,Phase::READY,2);assert(!strcmp(c.state,"Radio error"));
 c=radio(n,2,Phase::FAILED,2);assert(!strcmp(c.state,"Connection failed"));
 mcxgnss::Fix f;auto g=gps(f,true,0,false);assert(!strcmp(g.state,"Waiting for GPS")&&!g.valid);
 f.frames=5;g=gps(f,true,0,false);assert(!strcmp(g.state,"Searching / no fix"));
 f.valid=true;f.have_hdop=true;f.hdop=.6;g=gps(f,true,0,false);assert(!strcmp(g.state,"FIX GNSS")&&!strcmp(g.accuracy,"Accuracy unavailable"));
 f.have_accuracy=true;f.horizontal_accuracy=2.1;f.accuracy_source=1;g=gps(f,true,0,false);assert(!strcmp(g.accuracy,"Est. accuracy 2.1 m"));
 g=gps(f,true,0,true);assert(!strcmp(g.accuracy,"Est. accuracy 6.9 ft"));
 f.have_quality=true;f.quality=5;g=gps(f,true,0,false);assert(!strcmp(g.state,"RTK FLOAT"));
 f.quality=4;g=gps(f,true,0,false);assert(!strcmp(g.state,"RTK FIX"));
 f.accuracy_source=2;g=gps(f,true,0,false);assert(!strcmp(g.accuracy,"Est. hRMS 2.1 m"));
 g=gps(f,false,0,false);assert(!g.valid&&!strcmp(g.state,"GPS disabled")&&!strcmp(g.accuracy,"Accuracy unavailable"));
 g=gps(f,true,1,false);assert(!g.valid&&!strcmp(g.state,"GPS unavailable"));
 f.valid=false;g=gps(f,true,0,false);assert(!strcmp(g.state,"Searching / no fix")&&!strcmp(g.accuracy,"Accuracy unavailable"));
 puts("PASS Home presentation: correct network phases, independent known/unread counts, offline cached data, GNSS/RTK distinction and receiver-only accuracy");
}
