#include "ble_dual_stub.h"
#include "ble_under_test.h"
static void connect_mt(){using namespace meshble;
 assert(select(MESHTASTIC));Peer peer={};peer.advertised=MESHTASTIC;peer.addr.val[0]=11;strcpy(peer.name,"MT radio");mt::connect_selected(peer,0);assert(phase()==CONNECTING);
 ble_gap_event event;event.type=BLE_GAP_EVENT_LINK_ESTAB;event.connect.conn_handle=0;mt::gap_event(&event,nullptr);
 ble_gatt_svc service;service.uuid=mt::MT_SERVICE;ble_gatt_error error;mt::service_cb(0,&error,&service,mt::mc_token());error.status=BLE_HS_EDONE;mt::service_cb(0,&error,nullptr,mt::mc_token());
 event.type=BLE_GAP_EVENT_PASSKEY_ACTION;event.passkey.conn_handle=0;event.passkey.params.action=BLE_SM_IOACT_INPUT;mt::gap_event(&event,nullptr);assert(submit_pin(123456)&&pin_handle==0);assert(!select(MESHCORE));
 event.type=BLE_GAP_EVENT_ENC_CHANGE;event.enc_change.conn_handle=0;mt::gap_event(&event,nullptr);error.status=0;mt::mt_to=10;mt::mt_num=11;mt::mt_subscribed(0,&error,nullptr,mt::mt_token());assert(snapshot(MESHTASTIC).phase==READY);
}
static void connect_mc(){using namespace meshble;
 assert(select(MESHCORE));Peer peer={};peer.advertised=MESHCORE;peer.addr.val[0]=22;strcpy(peer.name,"MC radio");mc::connect_selected(peer,0);assert(phase()==CONNECTING);
 ble_gap_event event;event.type=BLE_GAP_EVENT_LINK_ESTAB;event.connect.conn_handle=1;mc::gap_event(&event,nullptr);
 ble_gatt_svc service;service.uuid=mc::MC_SERVICE;ble_gatt_error error;mc::service_cb(1,&error,&service,mc::mc_token());error.status=BLE_HS_EDONE;mc::service_cb(1,&error,nullptr,mc::mc_token());
 event.type=BLE_GAP_EVENT_ENC_CHANGE;event.enc_change.conn_handle=1;mc::gap_event(&event,nullptr);error.status=0;mc::mc_rx=20;mc::mc_tx=21;mc::mc_subscribed(1,&error,nullptr,mc::mc_token());assert(snapshot(MESHCORE).phase==READY);
}
int main(){using namespace meshble;
 descriptions[0].peer_id_addr.val[0]=11;descriptions[1].peer_id_addr.val[0]=22;
 assert(request_start());on_sync();connect_mt();const int mt_downs=meshmt::downs;connect_mc();assert(meshmt::downs==mt_downs);assert(snapshot(MESHTASTIC).phase==READY&&snapshot(MESHCORE).phase==READY);
 assert(nvs.count("peer_mt")&&nvs.count("peer_mc"));assert(mt::read_remembered()&&mc::read_remembered());assert(mt::remembered.radio==MESHTASTIC&&mc::remembered.radio==MESHCORE);
 const uint8_t payload[]={1,2};assert(mc::mc_write(payload,2)&&write_handle==1);assert(mt::mt_write(payload,2)&&write_handle==0);ble_gatt_error ok;mt::mt_written(0,&ok,nullptr,mt::mt_token());
 os_mbuf mb;mb.len=2;ble_gap_event event;event.type=BLE_GAP_EVENT_NOTIFY_RX;event.notify_rx={1,21,&mb};mc::gap_event(&event,nullptr);assert(meshmc::frames==1);event.notify_rx={0,11,&mb};mc::gap_event(&event,nullptr);assert(meshmc::frames==1);
 void *stale=mc::mc_token();assert(select(MESHCORE));disconnect();assert(mc::auto_paused&&!mt::auto_paused);assert(terminate_handle==1);
 event.type=BLE_GAP_EVENT_DISCONNECT;event.disconnect.conn.conn_handle=1;mc::gap_event(&event,nullptr);assert(snapshot(MESHTASTIC).phase==READY&&meshmt::downs==mt_downs);assert(!mc::reconnect_remembered());
 for(unsigned i=0;i<30;++i){connect_mc();const int writes=meshmc::writes;mc::mc_write_cb(1,&ok,nullptr,stale);assert(meshmc::writes==writes);assert(snapshot(MESHTASTIC).phase==READY&&meshmt::downs==mt_downs);stale=mc::mc_token();mc::gap_event(&event,nullptr);}
 // Migration: each lane accepts only its own legacy authenticated identity.
 nvs["last_peer"]=nvs["peer_mt"];nvs.erase("peer_mt");nvs.erase("peer_mc");mt::have_remembered=mc::have_remembered=false;assert(mt::read_remembered());assert(!mc::read_remembered());
 // Unsupported service on MC slot fails only that link.
 assert(select(MESHCORE));Peer peer={};mc::connect_selected(peer,0);event.type=BLE_GAP_EVENT_LINK_ESTAB;event.connect.conn_handle=1;mc::gap_event(&event,nullptr);ble_gatt_svc svc;svc.uuid=mc::MT_SERVICE;mc::service_cb(1,&ok,&svc,mc::mc_token());ble_gatt_error done;done.status=BLE_HS_EDONE;mc::service_cb(1,&done,nullptr,mc::mc_token());assert(mc::phase()==FAILED&&mt::phase()==READY);
 on_reset(5);assert(mt::conn==BLE_HS_CONN_HANDLE_NONE&&mc::conn==BLE_HS_CONN_HANDLE_NONE);
 puts("PASS: actual dual-link BLE callbacks, simultaneous GATT writes, pairing isolation, separate NVS identities, legacy migration, 30 reconnects, stale callback rejection and isolated failure");
}
