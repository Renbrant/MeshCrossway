#pragma once
// Included inside meshcrossway. Every model/UI access is on the LVGL owner task.
namespace cfgui {static void open_mt_channel(unsigned);}
namespace mtui {
  static lv_obj_t *dialog=nullptr,*value=nullptr,*keyboard=nullptr,*feedback=nullptr,*send_button=nullptr;
  static lv_obj_t *home_name=nullptr,*home_status=nullptr,*radio_status=nullptr;
  static meshmt::Target selected;
  static bool conversation=false;
  static uint32_t drawn=UINT32_MAX,last_draw=0,drawn_zone=0;
  static uint8_t meshcore_node_order[(meshmc::MAX_CONTACTS*9+7)/8]={};
  static uint16_t meshcore_order_get(unsigned i){const unsigned bit=i*9,byte=bit>>3,shift=bit&7;const uint16_t pair=uint16_t(meshcore_node_order[byte])|(uint16_t(meshcore_node_order[byte+1])<<8);return uint16_t((pair>>shift)&0x1ff);}
  static void meshcore_order_set(unsigned i,uint16_t value){const unsigned bit=i*9,byte=bit>>3,shift=bit&7;uint16_t pair=uint16_t(meshcore_node_order[byte])|(uint16_t(meshcore_node_order[byte+1])<<8);const uint16_t mask=uint16_t(0x1ffu<<shift);pair=uint16_t((pair&~mask)|((value&0x1ffu)<<shift));meshcore_node_order[byte]=uint8_t(pair);meshcore_node_order[byte+1]=uint8_t(pair>>8);}
  static uint16_t node_order[meshmt::MAX_NODES]={},channel_order[meshmt::MAX_CHANNELS]={};
  static lv_timer_t *node_batch_timer=nullptr;
  static lv_obj_t *node_list_view=nullptr;
  static bool nodes_were_syncing=false;
  static unsigned mt_node_count=0,mc_node_count=0,mt_node_cursor=0,mc_node_cursor=0,node_batch_rows=0;
  static int node_batch_network=0,node_batch_width=0;
  static bool ready(){auto *c=meshmt::client;
    return c&&c->connected&&c->initialized&&c->history_ready&&!c->fault;
  }
  static const char *name(uint32_t id){auto *c=meshmt::client;
    auto *n=c?c->node(id):nullptr;
    return n&&n->name[0]?n->name:"Unknown node";
  }
  static void compact_list_label(char *text){
    if(!text)return;
    for(char *p=text;*p;++p)if(*p=='\n'||*p=='\r'||*p=='\t')*p=' ';
  }
  static lv_obj_t *list_icon(lv_obj_t *parent,const char *glyph,int x,int y,int size,lv_color_t accent){
    auto *badge=panel_at(parent,x,y,size,size,lv_color_hex(0x302515),size/2);
    lv_obj_set_style_border_width(badge,1,0);
    lv_obj_set_style_border_color(badge,accent,0);
    auto *mark=txt(badge,glyph,0,(size-20)/2,size,accent);
    lv_obj_set_height(mark,20);
    lv_obj_set_style_text_align(mark,LV_TEXT_ALIGN_CENTER,0);
    return badge;
  }
  static uint8_t primary(){auto *c=meshmt::client;
    if(c)for(unsigned i=0;i<meshmt::MAX_CHANNELS;++i)if(c->data.channels[i].role==1)return i;
    return 0;
  }
  static void draft_save(){
    auto *c=meshmt::client;
    if(!c||!value)return;
    auto *d=c->draft(selected,true);
    const char *text=lv_textarea_get_text(value);const size_t n=strlen(text);
    if(n>meshmt::MAX_TEXT||!meshmt::utf8(text,n))return;
    if(d&&strcmp(d->text,text)){snprintf(d->text,sizeof(d->text),"%s",text);
      ++c->generation;
    }
  }
  static void close(){
    draft_save();
    if(keyboard)lv_keyboard_set_textarea(keyboard,nullptr);
    auto *old=dialog;
    dialog=value=keyboard=feedback=send_button=nullptr;
    if(old)lv_obj_del(old);
  }
  static void close_event(lv_event_t *){close();
  }
  static void list_now(void *){conversation=false;
    mcui::conversation=false;
    page=3;
    draw();
  }
  static void list_event(lv_event_t *){lv_async_call(list_now,nullptr);
  }
  static void send_event(lv_event_t *){
    auto *c=meshmt::client;
    if(!c||!value)return;
    draft_save();
    const char *text=lv_textarea_get_text(value);
    const char *problem=c->send_problem(selected,text);
    if(problem){lv_label_set_text(feedback,problem);
      return;
    }
    const uint32_t now=uint32_t(esp_timer_get_time()/1000);
    if(!c->send_text(selected,text,esp_random(),now)){lv_label_set_text(feedback,"Could not queue message. Wait and try again.");
      return;
    }
    if(auto *d=c->draft(selected)){d->text[0]=0;
      ++c->generation;
    }
    lv_textarea_set_text(value,"");
    close();
    draw();
  }
  static void compose(){
    auto *c=meshmt::client;
    if(!c||!conversation){notice("MESHTASTIC / COMPOSE","Choose a Meshtastic node or channel first.");
      return;
    }
    close();
    dialog=panel_at(lv_scr_act(),0,0,480,320,bg,0);
    txt(dialog,selected.node==meshmt::BROADCAST?"MESHTASTIC / CHANNEL":"MESHTASTIC / DIRECT",8,8,260,orange);
    action_button(dialog,"CLOSE",378,3,96,33,panel2,close_event,0);
    value=lv_textarea_create(dialog);
    lv_obj_set_pos(value,6,40);
    lv_obj_set_size(value,468,53);
    lv_textarea_set_max_length(value,meshmt::MAX_TEXT);
    lv_textarea_set_one_line(value,true);
    if(auto *d=c->draft(selected))lv_textarea_set_text(value,d->text);
    else lv_textarea_set_text(value,"");
    feedback=txt(dialog,"Checking native LoRa byte limit...",8,96,355,muted);
    send_button=action_button(dialog,"SEND",380,94,94,32,orange,send_event,0);
    keyboard=lv_keyboard_create(dialog);
    lv_obj_set_align(keyboard,LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(keyboard,4,132);
    lv_obj_set_size(keyboard,472,184);
    lv_keyboard_set_map(keyboard,LV_KEYBOARD_MODE_TEXT_LOWER,wifi_qwerty_lower,wifi_qwerty_lower_ctrl);
    lv_keyboard_set_map(keyboard,LV_KEYBOARD_MODE_TEXT_UPPER,wifi_qwerty_upper,wifi_qwerty_upper_ctrl);
    lv_keyboard_set_map(keyboard,LV_KEYBOARD_MODE_SPECIAL,wifi_qwerty_symbols,wifi_qwerty_symbols_ctrl);
    lv_keyboard_set_textarea(keyboard,value);
    // Keyboard DONE only closes editing; transmission always needs the SEND button.
    lv_obj_add_event_cb(keyboard,close_event,LV_EVENT_READY,nullptr);
    lv_obj_add_event_cb(keyboard,close_event,LV_EVENT_CANCEL,nullptr);
  }
  static void tools(){
    auto *c=meshmt::client;
    char info[900];
    if(!c){notice("MESHTASTIC / RADIO","More > Radios > Enable BLE > Scan. Use official Meshtastic BLE PhoneAPI with Bluetooth enabled, radio Wi-Fi off, and a compatible pairing mode. Close any competing clients.");
      return;
    }
    snprintf(info,sizeof(info),"%.105s\nFirmware: %.48s / HW: %lu / MTU: %u\nNode: !%08lx / region: %u / TX: %s\n%u nodes / %u messages / dropped nodes: %lu\nFromRadio: %lu frames / %lu packets / %lu text / %lu ignored\n%s\nOfficial Meshtastic BLE PhoneAPI required. For ESP32 radios, turn Wi-Fi OFF. Independent Meshtastic and MeshCore BLE sessions.",
    c->status,c->firmware,(unsigned long)c->hardware,c->mtu,(unsigned long)c->data.my_num,c->region,c->tx_enabled?"enabled":"disabled",
    c->data.node_count,c->data.message_count,(unsigned long)c->dropped_nodes,
    (unsigned long)c->rx_frames,(unsigned long)c->rx_packets,(unsigned long)c->rx_text,(unsigned long)c->ignored,
    meshmt::history_status());
    close();
    dialog=panel_at(lv_scr_act(),0,0,480,320,bg,0);
    txt(dialog,"MESHTASTIC / COMPATIBILITY",8,10,355,orange);
    action_button(dialog,"CLOSE",378,3,96,33,panel2,close_event,0);
    auto *scroller=panel_at(dialog,8,45,464,270,bg,0);
    lv_obj_add_flag(scroller,LV_OBJ_FLAG_SCROLLABLE);
    auto *label=txt(scroller,info,0,0,450,white);
    lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
    auto *tip=txt(scroller,"Channel names/keys, modem and region are read from your radio. Open Meshtastic Settings to read and change supported radio settings. PKI-capable radios require the destination key; other radios use native channel encryption. Remote ACK does not mean read.",0,220,450,muted);
    lv_label_set_long_mode(tip,LV_LABEL_LONG_WRAP);
  }
  static void tools_event(lv_event_t *e){cfgui::open_mt(e);
  }
  static void select_node_now(void *data){
    auto *c=meshmt::client;
    if(!c)return;
    uint32_t id=uint32_t(uintptr_t(data));
    selected=c->target(id,primary());
    conversation=true;
    active_chat_network=1;
    chats_filter=1;
    page=3;
    draw();
  }
  static void select_node(lv_event_t *e){lv_async_call(select_node_now,lv_event_get_user_data(e));
  }
  static void select_channel_now(void *data){
    auto *c=meshmt::client;
    if(!c)return;
    unsigned slot=unsigned(uintptr_t(data));
    if(slot>=meshmt::MAX_CHANNELS)return;
    selected=c->target(meshmt::BROADCAST,slot);
    conversation=true;
    active_chat_network=1;
    chats_filter=1;
    page=3;
    draw();
  }
  static void select_channel(lv_event_t *e){lv_async_call(select_channel_now,lv_event_get_user_data(e));
  }
  static void open_message_now(void *data){
    auto *c=meshmt::client;
    if(!c)return;
    uint32_t seq=uint32_t(uintptr_t(data));
    for(auto &m:c->data.messages)if(m.seq==seq){
      selected=c->target(m.to==meshmt::BROADCAST?meshmt::BROADCAST:m.outgoing?m.to:m.from,m.channel);
      selected.channel_identity=m.channel_identity;
      conversation=true;
      active_chat_network=1;
      chats_filter=1;
      page=3;
      draw();
      return;
    }
  }
  static void open_message(lv_event_t *e){lv_async_call(open_message_now,lv_event_get_user_data(e));
  }
  static void toggle_node_sort_now(void *){mcui::node_sort_distance=!mcui::node_sort_distance;draw();}
  static void toggle_node_sort(lv_event_t *){lv_async_call(toggle_node_sort_now,nullptr);}
  static void refresh_nodes_now(void *){if(page==1)draw();}
  static void refresh_nodes_event(lv_event_t *){lv_async_call(refresh_nodes_now,nullptr);}
  static void add_channel_now(void *) {
    auto *c=meshmt::client;
    if(!c||!c->connected||!c->initialized||!c->history_ready||c->fault){notice("MESHTASTIC / CHANNEL","Connect and synchronize this radio first.");return;}
    for(unsigned slot=1;slot<meshmt::MAX_CHANNELS;++slot)
      if(!c->data.channels[slot].loaded||!c->data.channels[slot].role){cfgui::open_mt_channel(slot);return;}
    notice("MESHTASTIC / CHANNEL","No unused secondary channel slots were reported by this radio.");
  }
  static void add_channel(lv_event_t *){lv_async_call(add_channel_now,nullptr);}
  static void node_details(){
    auto *c=meshmt::client;
    auto *n=c?c->node(selected.node):nullptr;
    if(!n){tools();
      return;
    }char info[450];
    snprintf(info,sizeof(info),"%s / !%08lx\nHW: %lu / last heard: %lu Unix\nPosition: %s / %.7f, %.7f / %ld m\nBattery: %s%u / %.2f V / channel use %.1f%%\nPKI key: %s / %s",n->name,(unsigned long)n->num,(unsigned long)n->hardware,(unsigned long)n->last_heard,
    n->position?"available":"unavailable",n->latitude/1e7,n->longitude/1e7,(long)n->altitude,n->has_battery?"":"unknown ",n->battery,n->voltage,n->utilization,n->has_key?"available":"not received",n->unmessagable?"cannot receive messages":"message capable");
    notice("MESHTASTIC / NODE",info);
  }
  static bool belongs(const meshmt::Message &m){
    return meshmt::client&&meshmt::message_belongs(m,selected,meshmt::client->data.my_num);
  }
  static void node_batch_tick(lv_timer_t *timer){
    if(page!=1||!showing_mt()){lv_timer_del(timer);node_batch_timer=nullptr;return;}
    unsigned budget=3;
    while(budget){
      if(node_batch_network==1&&mt_node_cursor>=mt_node_count)node_batch_network=2;
      if(node_batch_network==2&&mc_node_cursor>=mc_node_count)node_batch_network=0;
      if(node_batch_network==0){
        lv_timer_del(timer);node_batch_timer=nullptr;
        auto *mt=meshmt::client;auto *mc=meshmc::client;
        drawn=mt?mt->generation:0;drawn_zone=mcxchat::time_zone_key();
        mcui::rendered_generation=mc?mc->generation:0;
        return;
      }
      char title[72],detail[128],when[24];
      uint32_t last=0;uintptr_t token=0;lv_event_cb_t callback=nullptr;const char *glyph="P";
      if(node_batch_network==1){
        auto *c=meshmt::client;if(!c){node_batch_network=2;continue;}
        const unsigned index=node_order[mt_node_cursor++];if(index>=c->data.node_count)continue;
        const auto &n=c->data.nodes[index];last=n.last_heard;
        snprintf(title,sizeof(title),"%s",n.name[0]?n.name:n.short_name[0]?n.short_name:"!node");
        compact_list_label(title);token=n.num;callback=select_node;
        const uint8_t role=c->node_role(n.num);
        const bool repeater=role==2||role==3||role==4||role==11||role==12;
        const char *kind=repeater?"Meshtastic repeater":role==5?"Meshtastic tracker":role==6?"Meshtastic sensor":"Meshtastic person";
        node_heard_age(last,when,sizeof(when));
        if(mcui::node_sort_distance&&mcui::nav_gps_valid&&n.has_lat&&n.has_lon){
          const double lat=double(n.latitude)/1e7,lon=double(n.longitude)/1e7;
          const double dy=(lat-mcui::nav_latitude)*111320.0;
          const double dx=(lon-mcui::nav_longitude)*111320.0*cos(mcui::nav_latitude*0.017453292519943295);
          snprintf(detail,sizeof(detail),"ID %04X / %s / %s / %.0f m",unsigned(n.num&0xffffu),kind,when,sqrt(dx*dx+dy*dy));
        }else snprintf(detail,sizeof(detail),"ID %04X / %s / %s",unsigned(n.num&0xffffu),kind,when);
        if(repeater)glyph="R";else if(role==5)glyph="T";else if(role==6)glyph="S";
      }else{
        auto *c=meshmc::client;if(!c){node_batch_network=0;continue;}
        const unsigned index=meshcore_order_get(mc_node_cursor++);if(index>=c->contact_count)continue;
        const auto &contact=c->contacts[index];last=mcui::contact_last_heard(*c,int(index));
        snprintf(title,sizeof(title),"%s",contact.name[0]?contact.name:"Unknown node");
        compact_list_label(title);token=index;callback=mcui::choose_contact;
        const char *kind=contact.type==2?"MeshCore repeater":contact.type==3?"MeshCore room":contact.type==4?"MeshCore sensor":"MeshCore companion";
        const char *route=contact.path_len==255?"Flood":"Direct";
        node_heard_age(last,when,sizeof(when));
        if(mcui::node_sort_distance&&mcui::nav_gps_valid&&contact.lat&&contact.lon){
          const double lat=double(contact.lat)/1e6,lon=double(contact.lon)/1e6;
          const double dy=(lat-mcui::nav_latitude)*111320.0;
          const double dx=(lon-mcui::nav_longitude)*111320.0*cos(mcui::nav_latitude*0.017453292519943295);
          snprintf(detail,sizeof(detail),"ID %02X%02X / %s / %s / %s / %.0f m",contact.key[4],contact.key[5],kind,route,when,sqrt(dx*dx+dy*dy));
        }else snprintf(detail,sizeof(detail),"ID %02X%02X / %s / %s / %s",contact.key[4],contact.key[5],kind,route,when);
        glyph=contact.type==2?"R":contact.type==3?"H":contact.type==4?"S":"P";
      }
      const int row_y=int(node_batch_rows++*64u);
      auto *b=action_button(node_list_view,"",0,row_y,node_batch_width,58,panel,callback,intptr_t(token));
      mtui::list_icon(b,glyph,8,9,40,orange);
      auto *name=lv_obj_get_child(b,0);mcui::emoji_label_text(name,title);
      lv_obj_set_pos(name,56,5);lv_obj_set_size(name,node_batch_width-64,20);
      lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_LEFT,0);
      auto *heard_label=txt(b,detail,56,30,node_batch_width-64,muted);
      lv_obj_set_height(heard_label,18);lv_label_set_long_mode(heard_label,LV_LABEL_LONG_DOT);
      --budget;
    }
  }
  static void render(){
    if(node_batch_timer){lv_timer_del(node_batch_timer);node_batch_timer=nullptr;}
    mt_node_count=mc_node_count=mt_node_cursor=mc_node_cursor=node_batch_rows=0;node_batch_network=0;
    const int w=lv_obj_get_width(body);
    node_list_view=nullptr;
    auto *c=meshmt::client;
    drawn=c?c->generation:0;drawn_zone=mcxchat::time_zone_key();
    mcui::rendered_generation=meshmc::client?meshmc::client->generation:0;
    if(page==3&&conversation&&active_chat_network==1&&chats_filter!=2){
      const char *title=selected.node==meshmt::BROADCAST&&c&&selected.channel<meshmt::MAX_CHANNELS?c->data.channels[selected.channel].name:name(selected.node);
      const bool cached=chatview::active(body);
      if(!chatview::begin(body))return;
      chatview::header(title,ready()?"Meshtastic / live":"Meshtastic / saved history",meshmt::history_status());
      if(!cached)action_button(body,"< CHATS",8,54,100,32,panel2,list_event,0);
      if(c&&!memcmp(selected.radio,c->data.radio,7)){
        const unsigned stored=mcxchat::ordered_indices(c->data.messages,chatview::cache->order,96,[](const meshmt::Message &m){return m.seq;});
        for(unsigned row=0;row<stored;++row){auto &m=c->data.messages[chatview::cache->order[row]];
          if(!belongs(m))continue;
          if(!m.read){m.read=true;
            ++c->generation;
          }
          char meta[112];
          char clock[16];
          mcxchat::time_local(m.time,clock,sizeof(clock));
          snprintf(meta,sizeof(meta),"%s / %s / %s%s",clock,meshmt::delivery_name(m.delivery),m.pki?"PKI":"channel",m.via_mqtt?" / MQTT":"");
          chatview::row(m.seq,chatview::hash(m.outgoing?"You":name(m.from),m.text,meta,m.outgoing),[&](int y){
            auto *card=panel_at(body,8,y,w-16,104,panel,7);
            mcui::emoji_txt(card,m.outgoing?"You":name(m.from),8,6,w-32,m.outgoing?orange:green);
            auto *text=mcui::emoji_txt(card,m.text,8,30,w-32,white);
            lv_label_set_long_mode(text,LV_LABEL_LONG_WRAP);
            lv_obj_update_layout(text);
            const int footer=lv_obj_get_height(text)+40;
            single_label(card,meta,8,footer,w-32,muted);
            lv_obj_set_height(card,footer+30);
            return card;
          });
        }
      }
      drawn=c?c->generation:0;drawn_zone=mcxchat::time_zone_key();
      chatview::end("No messages in this conversation.");
      return;
    }
    const int filter=page==1?nodes_filter:page==2?groups_filter:chats_filter;
    const bool syncing=filter!=2&&c&&c->connected&&(!c->initialized||!c->history_ready);
    if(page==1)nodes_were_syncing=syncing;
    show_center_heading(page==1?"NODES / NETWORKS":page==2?"GROUPS / CHANNELS":"CHATS / NETWORKS",page==1?(syncing?"Synchronizing Meshtastic nodes…":ready()?"Meshtastic connected":"More > Radios > Enable BLE"):ready()?"Meshtastic connected":c?c->status:"More > Radios > Enable BLE");
    filter_tabs(w,filter);
    if(page==1){const int control_width=(w-24)/2;action_button(body,mcui::node_sort_label(),8,98,control_width,34,panel2,toggle_node_sort,0);action_button(body,"REFRESH",16+control_width,98,control_width,34,panel2,refresh_nodes_event,0);lv_obj_clear_flag(body,LV_OBJ_FLAG_SCROLLABLE);node_list_view=list_view_at(body,141,w);}
    else if(page==2)action_button(body,"ADD CHANNEL",8,98,w-16,34,panel2,add_channel,0);
    int y=page==3?98:141;
    unsigned index=0,shown=0;
    auto row=[&](const char *title,const char *detail,lv_event_cb_t callback,uintptr_t token){
      ++index;++shown;
      auto *b=action_button(body,"",8,y,w-16,page==1?58:page==2?58:74,panel,callback,intptr_t(token));
      if(page==1){
        auto *name=lv_obj_get_child(b,0);mcui::emoji_label_text(name,title);
        const int card_width=w-16;const int name_width=card_width>=350?145:card_width*30/100;const int detail_x=name_width+14;
        lv_obj_set_pos(name,7,18);lv_obj_set_size(name,name_width,22);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_LEFT,0);
        auto *d=txt(b,detail,detail_x,4,card_width-detail_x-8,muted);lv_obj_set_height(d,50);lv_label_set_long_mode(d,LV_LABEL_LONG_WRAP);
        y+=64;
      }else if(page==2){
        char compact_title[100];snprintf(compact_title,sizeof(compact_title),"%s",title);compact_list_label(compact_title);
        auto *name=lv_obj_get_child(b,0);mcui::emoji_label_text(name,compact_title);
        const int card_width=w-16;
        mtui::list_icon(b,"#",8,9,40,orange);
        lv_obj_set_pos(name,56,5);lv_obj_set_size(name,card_width-64,20);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_LEFT,0);
        auto *d=txt(b,detail,56,30,card_width-64,muted);lv_obj_set_height(d,18);lv_label_set_long_mode(d,LV_LABEL_LONG_DOT);
        y+=64;
      }else{
        char compact_title[100];snprintf(compact_title,sizeof(compact_title),"%s",title);compact_list_label(compact_title);
        auto *name=lv_obj_get_child(b,0);mcui::emoji_label_text(name,compact_title);
        lv_obj_set_pos(name,7,5);lv_obj_set_size(name,w-30,20);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_LEFT,0);
        auto *d=txt(b,detail,7,29,w-30,muted);lv_obj_set_height(d,38);lv_label_set_long_mode(d,LV_LABEL_LONG_WRAP);
        y+=80;
      }
    };
    if(c&&filter!=2){
      if(page==1){unsigned count=0;if(!syncing)for(unsigned i=0;i<c->data.node_count;++i){const auto &n=c->data.nodes[i];if(!c->keep_node_index(i))continue;node_order[count++]=uint16_t(i);}
        const bool gps_valid=mcui::nav_gps_valid;const double gps_latitude=mcui::nav_latitude,gps_longitude=mcui::nav_longitude;
        auto distance=[&](unsigned i){const auto &n=c->data.nodes[i];if(!gps_valid||!n.has_lat||!n.has_lon)return 1.0e30;
          const double lat=double(n.latitude)/1e7,lon=double(n.longitude)/1e7;
          const double dy=(lat-gps_latitude)*111320.0,dx=(lon-gps_longitude)*111320.0*cos(gps_latitude*0.017453292519943295);
          return dx*dx+dy*dy;
        };
        auto heard=[&](unsigned i){return c->data.nodes[i].last_heard;};
        auto before=[&](unsigned a,unsigned b){return mcui::node_sort_distance&&mcui::nav_gps_valid?distance(a)<distance(b):heard(a)>heard(b);};
        auto sort=[&](auto&&self,int lo,int hi)->void{int i=lo,j=hi;const uint16_t pivot=node_order[(lo+hi)/2];
          while(i<=j){while(before(node_order[i],pivot))++i;while(before(pivot,node_order[j]))--j;
            if(i<=j){const uint16_t swap=node_order[i];node_order[i]=node_order[j];node_order[j]=swap;++i;--j;}}
          if(lo<j)self(self,lo,j);
          if(i<hi)self(self,i,hi);};
        if(count>1)sort(sort,0,int(count)-1);
        if(count>NODE_MAX_RECENT)count=NODE_MAX_RECENT;
        mt_node_count=count;
      }
      if(page==2&&filter==0&&meshmc::client){
        unsigned direct_unread=0,channel_unread=0;
        for(const auto &m:meshmc::client->messages)if(m.id&&!m.outgoing&&!m.read){if(m.channel)++channel_unread;else ++direct_unread;}
        char summary[90];snprintf(summary,sizeof(summary),"MeshCore unread: %u direct / %u channels",direct_unread,channel_unread);
        single_label(body,summary,8,y,w-16,muted);y+=23;
      }
      if(page==2){unsigned count=0;uint32_t latest[meshmt::MAX_CHANNELS]={};uint16_t unread[meshmt::MAX_CHANNELS]={};uint32_t latest_time[meshmt::MAX_CHANNELS]={};
        for(unsigned i=0;i<meshmt::MAX_CHANNELS;++i){const auto &ch=c->data.channels[i];if(!ch.loaded||!ch.role)continue;channel_order[count++]=uint16_t(i);
          for(const auto &m:c->data.messages)if(m.seq&&!m.outgoing&&m.to==meshmt::BROADCAST&&m.channel==i&&m.channel_identity==ch.identity){
            if(m.seq>latest[i]){latest[i]=m.seq;latest_time[i]=m.time;}
            if(!m.read&&unread[i]<UINT16_MAX)++unread[i];
          }
        }
        for(unsigned a=1;a<count;++a){const uint16_t value=channel_order[a];unsigned b=a;while(b&&latest[channel_order[b-1]]<latest[value]){channel_order[b]=channel_order[b-1];--b;}channel_order[b]=value;}
        for(unsigned i=0;i<count;++i){const unsigned slot=channel_order[i];auto &ch=c->data.channels[slot];char title[70],detail[96],when[20];
          snprintf(title,sizeof(title),"MT / %u / %s",slot,ch.name);
          mcxchat::time_local(latest_time[slot],when,sizeof(when));
          snprintf(detail,sizeof(detail),"%u unread | %s",unread[slot],latest[slot]?when:"--");
          row(title,detail,select_channel,slot);
        }
      }
      if(page==3){
        for(unsigned k=0;k<c->data.message_count;++k){const uint32_t seq=c->data.next_seq-1-k;
          auto &m=c->data.messages[(seq-1)%meshmt::MAX_MESSAGES];
          if(m.seq!=seq)continue;
          bool later=false;
          for(const auto &n:c->data.messages)if(n.seq>m.seq&&n.channel==m.channel&&n.channel_identity==m.channel_identity&&
          (m.to==meshmt::BROADCAST?n.to==meshmt::BROADCAST:n.to!=meshmt::BROADCAST&&(n.outgoing?n.to:n.from)==(m.outgoing?m.to:m.from))){later=true;
            break;
          }
          if(later)continue;
          char title[90];
          snprintf(title,sizeof(title),"MT / %s",m.to==meshmt::BROADCAST?c->data.channels[m.channel].name:name(m.outgoing?m.to:m.from));
          row(title,m.text,open_message,m.seq);
        }
      }
    }
    // ALL includes both models, with native callbacks and a visible network label.
    auto *mc=meshmc::client;
    if(filter==0&&mc){
      if(page==1){unsigned count=0;for(unsigned i=0;i<mc->contact_count;++i){const auto &contact=mc->contacts[i];const uint32_t last=mcui::contact_last_heard(*mc,int(i));const auto key=mcxchat::key_for(*mc,false,uint16_t(i));const auto *pref=mcxhistory::preferences().get(key);if(!mc->keep_contact(contact))continue;meshcore_order_set(count++,uint16_t(i));}
        const bool gps_valid=mcui::nav_gps_valid;const double gps_latitude=mcui::nav_latitude,gps_longitude=mcui::nav_longitude;
        auto heard=[&](unsigned i){const auto &contact=mc->contacts[i];return contact.advert>contact.lastmod?contact.advert:contact.lastmod;};
        auto distance=[&](unsigned i){const auto &c=mc->contacts[i];if(!gps_valid||!c.lat||!c.lon)return 1.0e30;
          const double lat=double(c.lat)/1e6,lon=double(c.lon)/1e6;
          const double dy=(lat-gps_latitude)*111320.0,dx=(lon-gps_longitude)*111320.0*cos(gps_latitude*0.017453292519943295);return dx*dx+dy*dy;};
        auto before=[&](unsigned a,unsigned b){return mcui::node_sort_distance&&mcui::nav_gps_valid?distance(a)<distance(b):heard(a)>heard(b);};
        auto sort=[&](auto&&self,int lo,int hi)->void{int i=lo,j=hi;const uint16_t pivot=meshcore_order_get(unsigned((lo+hi)/2));
          while(i<=j){while(before(meshcore_order_get(unsigned(i)),pivot))++i;while(before(pivot,meshcore_order_get(unsigned(j))))--j;
            if(i<=j){const uint16_t swap=meshcore_order_get(unsigned(i));meshcore_order_set(unsigned(i),meshcore_order_get(unsigned(j)));meshcore_order_set(unsigned(j),swap);++i;--j;}}
          if(lo<j)self(self,lo,j);
          if(i<hi)self(self,i,hi);};
        if(count>1)sort(sort,0,int(count)-1);
        if(count>NODE_MAX_RECENT)count=NODE_MAX_RECENT;
        mc_node_count=count;
      }
      if(page==2)for(unsigned i=0;i<mc->channel_count;++i){auto &ch=mc->channels[i];
        if(!ch.loaded||!ch.name[0])continue;
        auto key=mcxchat::key_for(*mc,true,i);unsigned unread=0;uint32_t last=0;
        for(const auto &m:mc->messages)if(m.id&&mcxchat::belongs(m,key)){if(!m.outgoing&&m.observed_at>last)last=m.observed_at;if(!m.outgoing&&!m.read)++unread;}
        char title[75],detail[90],when[20];snprintf(title,sizeof(title),"MC / %s",ch.name);mcxchat::time_local(last,when,sizeof(when));
        snprintf(detail,sizeof(detail),"%u unread | %s",unread,last?when:"--");
        row(title,detail,mcui::choose_channel,i);
      }
      if(page==3&&mcxhistory::init_workspace()){
        mcui::thread_rows=mcxhistory::workspace()->rows;
        mcui::thread_count=mcxchat::list(*mc,mcxhistory::preferences(),mcui::thread_rows,meshmc::MAX_MESSAGES);
        for(unsigned i=0;i<mcui::thread_count;++i){const auto &t=mcui::thread_rows[i];
          auto *m=mc->by_id(t.last);
          char title[90];
          snprintf(title,sizeof(title),"MC / %s",mcui::thread_name(t.key));
          row(title,m?m->text:"Draft",mcui::open_row,i);
        }
      }
    }
    if(page==1){
      if(!mt_node_count&&!mc_node_count)
        single_label(node_list_view,syncing?"Syncing Meshtastic node database…":"No nodes found for this network yet.",8,8,w-32,syncing?orange:muted);
      if(mc_node_count||(!syncing&&mt_node_count)){
        node_batch_width=w-16;mt_node_cursor=mc_node_cursor=node_batch_rows=0;
        node_batch_network=mt_node_count?1:2;
        node_batch_timer=lv_timer_create(node_batch_tick,25,nullptr);
        node_batch_tick(node_batch_timer);
      }
      return;
    }
    if(!shown)single_label(body,"No entries for this network yet.",8,y,w-16,muted);
  }
  static void poll(lv_timer_t *){
    meshmt::poll();
    if(home_name)chatview::label(home_name,ready()?name(meshmt::client->data.my_num):"BLE: OFF");
    if(home_status)chatview::label(home_status,ready()?"Meshtastic connected":"Connect in More / Radios");
    if(radio_status&&meshmt::client)chatview::label(radio_status,meshmt::client->status);

    if(page==3&&conversation&&active_chat_network==1&&chatview::active(body))chatview::label(chatview::cache->history,meshmt::history_status());
    auto *c=meshmt::client;
    if(dialog&&value&&c){draft_save();
      const char *problem=c->send_problem(selected,lv_textarea_get_text(value));
      if(problem)lv_obj_add_state(send_button,LV_STATE_DISABLED);
      else lv_obj_clear_state(send_button,LV_STATE_DISABLED);
      char detail[140];
      snprintf(detail,sizeof(detail),"%u / %u bytes%s%s",unsigned(strlen(lv_textarea_get_text(value))),unsigned(c->text_limit(selected)),problem?" / ":"",problem?problem:"");
      chatview::label(feedback,detail);
    }
    if(!showing_mt()||dialog||mcui::dialog||wifi_dialog||ble_dialog||info_dialog||page<1||page>3)return;
    const uint32_t generation=c?c->generation:0;
    if(node_batch_timer){drawn=generation;drawn_zone=mcxchat::time_zone_key();if(meshmc::client)mcui::rendered_generation=meshmc::client->generation;return;}
    if(page==1){
      const bool syncing=nodes_filter!=2&&c&&c->connected&&(!c->initialized||!c->history_ready);
      if(nodes_were_syncing&&!syncing){nodes_were_syncing=false;draw();return;}
      nodes_were_syncing=syncing;drawn=generation;drawn_zone=mcxchat::time_zone_key();if(meshmc::client)mcui::rendered_generation=meshmc::client->generation;return;
    }
    if(generation==drawn&&drawn_zone==mcxchat::time_zone_key()&&(!meshmc::client||meshmc::client->generation==mcui::rendered_generation))return;
    for(lv_indev_t *input=lv_indev_get_next(nullptr);input;input=lv_indev_get_next(input))if(input->proc.state==LV_INDEV_STATE_PRESSED)return;
    const uint32_t now=uint32_t(esp_timer_get_time()/1000);
    if(!chatview::interacting(body)&&uint32_t(now-last_draw)>=250){last_draw=now;
      if(page==3&&conversation&&active_chat_network==1&&chatview::active(body))render();else draw();
    }
  }
} // namespace mtui
