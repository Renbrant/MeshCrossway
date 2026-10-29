#pragma once
// Included inside meshcrossway. Every model/UI access is on the LVGL owner task.
namespace mtui {
  static lv_obj_t *dialog=nullptr,*value=nullptr,*keyboard=nullptr,*feedback=nullptr,*send_button=nullptr;
  static lv_obj_t *home_name=nullptr,*home_status=nullptr,*radio_status=nullptr;
  static meshmt::Target selected;
  static bool conversation=false;
  static uint32_t drawn=UINT32_MAX,last_draw=0;
  static unsigned offset=0;
  static bool ready(){auto *c=meshmt::client;
    return c&&c->connected&&c->initialized&&c->history_ready&&!c->fault;
  }
  static const char *name(uint32_t id){auto *c=meshmt::client;
    auto *n=c?c->node(id):nullptr;
    return n&&n->name[0]?n->name:"Unknown node";
  }
  static uint8_t primary(){auto *c=meshmt::client;
    if(c)for(unsigned i=0;i<meshmt::MAX_CHANNELS;++i)if(c->data.channels[i].role==1)return i;
    return 0;
  }
  static void draft_save(){
    auto *c=meshmt::client;
    if(!c||!value)return;
    auto *d=c->draft(selected,true);
    if(d&&strcmp(d->text,lv_textarea_get_text(value))){snprintf(d->text,sizeof(d->text),"%s",lv_textarea_get_text(value));
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
    txt(dialog,"MESHTASTIC / MESSAGE",8,8,260,orange);
    action_button(dialog,"CLOSE",378,3,96,33,panel2,close_event,0);
    value=lv_textarea_create(dialog);
    lv_obj_set_pos(value,6,40);
    lv_obj_set_size(value,468,53);
    lv_textarea_set_max_length(value,meshmt::MAX_TEXT);
    lv_textarea_set_one_line(value,true);
    if(auto *d=c->draft(selected))lv_textarea_set_text(value,d->text);
    else lv_textarea_set_text(value,"");
    feedback=txt(dialog,"Up to 237 UTF-8 bytes / native Meshtastic",8,96,355,muted);
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
    if(!c){notice("MESHTASTIC / T-DECK","More > Radios > Enable BLE > Scan. On T-Deck use official Meshtastic, Bluetooth Programming Mode or BaseUI, Wi-Fi OFF, and the displayed PIN. Close the phone app.");
      return;
    }
    snprintf(info,sizeof(info),"%.105s\nFirmware: %.48s / HW: %lu / MTU: %u\nNode: !%08lx / region: %u / TX: %s\n%u nodes / %u messages / dropped nodes: %lu\n%s\nT-Deck: BaseUI or Bluetooth Programming Mode, Wi-Fi OFF. One active BLE radio on this Core.",
    c->status,c->firmware,(unsigned long)c->hardware,c->mtu,(unsigned long)c->data.my_num,c->region,c->tx_enabled?"enabled":"disabled",
    c->data.node_count,c->data.message_count,(unsigned long)c->dropped_nodes,meshmt::history_status());
    close();
    dialog=panel_at(lv_scr_act(),0,0,480,320,bg,0);
    txt(dialog,"MESHTASTIC / COMPATIBILITY",8,10,355,orange);
    action_button(dialog,"CLOSE",378,3,96,33,panel2,close_event,0);
    auto *scroller=panel_at(dialog,8,45,464,270,bg,0);
    lv_obj_add_flag(scroller,LV_OBJ_FLAG_SCROLLABLE);
    auto *label=txt(scroller,info,0,0,450,white);
    lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
    auto *tip=txt(scroller,"Channel names/keys, modem and region are read from your radio. Change those settings with the official Meshtastic client. PKI is requested when the destination key is available; other DMs use channel encryption. Remote ACK does not mean read.",0,220,450,muted);
    lv_label_set_long_mode(tip,LV_LABEL_LONG_WRAP);
  }
  static void tools_event(lv_event_t *){tools();
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
  static void paginate_now(void *data){
    const int step=int(intptr_t(data));
    if(step<0)offset=offset>=10?offset-10:0;
    else offset+=10;
    draw();
  }
  static void paginate(lv_event_t *e){lv_async_call(paginate_now,lv_event_get_user_data(e));
  }
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
    return m.seq&&m.channel==selected.channel&&m.channel_identity==selected.channel_identity&&
    (selected.node==meshmt::BROADCAST?m.to==meshmt::BROADCAST:m.to!=meshmt::BROADCAST&&(m.outgoing?m.to:m.from)==selected.node);
  }
  static void render(){
    const int w=lv_obj_get_width(body);
    auto *c=meshmt::client;
    drawn=c?c->generation:0;
    mcui::rendered_generation=meshmc::client?meshmc::client->generation:0;
    if(page==3&&conversation&&active_chat_network==1&&chats_filter!=2){
      const char *title=selected.node==meshmt::BROADCAST&&c&&selected.channel<meshmt::MAX_CHANNELS?c->data.channels[selected.channel].name:name(selected.node);
      show_center_heading(title,ready()?"Meshtastic / live":"Meshtastic / saved history");
      action_button(body,"< CHATS",8,54,100,32,panel2,list_event,0);
      single_label(body,meshmt::history_status(),114,61,w-122,muted);
      int y=96;
      if(c&&!memcmp(selected.radio,c->data.radio,7)){
        uint32_t first=c->data.next_seq>meshmt::MAX_MESSAGES?c->data.next_seq-meshmt::MAX_MESSAGES:1;
        for(uint32_t seq=first;seq<c->data.next_seq;++seq){auto &m=c->data.messages[(seq-1)%meshmt::MAX_MESSAGES];
          if(m.seq!=seq||!belongs(m))continue;
          if(!m.read){m.read=true;
            ++c->generation;
          }
          auto *card=panel_at(body,8,y,w-16,104,panel,7);
          mcui::emoji_txt(card,m.outgoing?"You":name(m.from),8,6,w-32,m.outgoing?orange:green);
          auto *text=mcui::emoji_txt(card,m.text,8,30,w-32,white);
          lv_label_set_long_mode(text,LV_LABEL_LONG_WRAP);
          lv_obj_update_layout(text);
          const int footer=lv_obj_get_height(text)+40;
          char meta[112];
          char clock[16];
          mcxchat::time_utc(m.time,clock,sizeof(clock));
          snprintf(meta,sizeof(meta),"%s / %s / %s%s",clock,meshmt::delivery_name(m.delivery),m.pki?"PKI":"channel",m.via_mqtt?" / MQTT":"");
          single_label(card,meta,8,footer,w-32,muted);
          lv_obj_set_height(card,footer+30);
          y+=footer+38;
        }
      }
      if(y==96)single_label(body,"No messages in this conversation.",8,y,w-16,muted);
      lv_obj_scroll_to_y(body,y,LV_ANIM_OFF);
      return;
    }
    const int filter=page==1?nodes_filter:page==2?groups_filter:chats_filter;
    show_center_heading(page==1?"NODES / NETWORKS":page==2?"GROUPS / CHANNELS":"CHATS / NETWORKS",ready()?"Meshtastic connected":c?c->status:"More > Radios > Enable BLE");
    filter_tabs(w,filter);
    action_button(body,"<",8,98,45,34,panel2,paginate,-1);
    action_button(body,">",59,98,45,34,panel2,paginate,1);
    action_button(body,"MT STATUS",112,98,w-120,34,panel2,tools_event,0);
    int y=141;
    unsigned index=0,shown=0;
    auto row=[&](const char *title,const char *detail,lv_event_cb_t callback,uintptr_t token){
      const unsigned current=index++;
      if(current<offset||shown>=10)return;
      ++shown;
      auto *b=action_button(body,"",8,y,w-16,64,panel,callback,intptr_t(token));
      mcui::emoji_txt(b,title,7,5,w-30,white);
      single_label(b,detail,7,35,w-30,muted);
      y+=71;
    };
    if(c&&filter!=2){
      if(page==1)for(unsigned i=0;i<c->data.node_count;++i){auto &n=c->data.nodes[i];
        char title[70],detail[100];
        snprintf(title,sizeof(title),"MT / %s",n.name);
        snprintf(detail,sizeof(detail),"!%08lx / %s%s",(unsigned long)n.num,n.position?"position available":"no position",n.num==c->data.my_num?" / your radio":"");
        row(title,detail,select_node,n.num);
      }
      if(page==2)for(unsigned i=0;i<meshmt::MAX_CHANNELS;++i){auto &ch=c->data.channels[i];
        if(!ch.loaded||!ch.role)continue;
        char title[70];
        snprintf(title,sizeof(title),"MT / %u / %s",i,ch.name);
        row(title,ch.role==1?"Primary channel":"Secondary channel",select_channel,i);
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
      if(page==1)for(unsigned i=0;i<mc->contact_count;++i){char title[75];
        snprintf(title,sizeof(title),"MC / %s",mc->contacts[i].name);
        row(title,"MeshCore contact",mcui::choose_contact,i);
      }
      if(page==2)for(unsigned i=0;i<mc->channel_count;++i){auto &ch=mc->channels[i];
        if(!ch.loaded||!ch.name[0])continue;
        char title[75];
        snprintf(title,sizeof(title),"MC / %s",ch.name);
        row(title,"MeshCore channel",mcui::choose_channel,i);
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
    if(!shown){if(offset&&index){offset=0;
        lv_async_call([](void *){draw();},nullptr);
      }else single_label(body,"No entries for this network yet.",8,y,w-16,muted);
    }
  }
  static void poll(lv_timer_t *){
    meshmt::poll();
    if(home_name)lv_label_set_text(home_name,ready()?name(meshmt::client->data.my_num):"BLE: OFF");
    if(home_status)lv_label_set_text(home_status,ready()?"Meshtastic connected":"Connect in More / Radios");
    if(radio_status&&meshmt::client)lv_label_set_text(radio_status,meshmt::client->status);

    auto *c=meshmt::client;
    if(dialog&&value&&c){draft_save();
      const char *problem=c->send_problem(selected,lv_textarea_get_text(value));
      if(problem)lv_obj_add_state(send_button,LV_STATE_DISABLED);
      else lv_obj_clear_state(send_button,LV_STATE_DISABLED);
      char detail[140];
      snprintf(detail,sizeof(detail),"%u / 237 bytes%s%s",unsigned(strlen(lv_textarea_get_text(value))),problem?" / ":"",problem?problem:"");
      lv_label_set_text(feedback,detail);
    }
    if(!showing_mt()||dialog||mcui::dialog||wifi_dialog||ble_dialog||info_dialog||page<1||page>3)return;
    const uint32_t generation=c?c->generation:0;
    if(generation==drawn&&(!meshmc::client||meshmc::client->generation==mcui::rendered_generation))return;
    for(lv_indev_t *input=lv_indev_get_next(nullptr);input;input=lv_indev_get_next(input))if(input->proc.state==LV_INDEV_STATE_PRESSED)return;
    const uint32_t now=uint32_t(esp_timer_get_time()/1000);
    if(uint32_t(now-last_draw)>=250){last_draw=now;
      draw();
    }
  }
} // namespace mtui
