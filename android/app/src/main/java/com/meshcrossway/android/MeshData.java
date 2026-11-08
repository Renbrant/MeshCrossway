package com.meshcrossway.android;

import java.util.*;

final class MeshData {
    static final class Node {
        String id="",name="",key="";long heard;double lat,lon;boolean position,unmessagable;int battery=-1,type;
        Node(){} Node(Node n){id=n.id;name=n.name;key=n.key;heard=n.heard;lat=n.lat;lon=n.lon;position=n.position;unmessagable=n.unmessagable;battery=n.battery;type=n.type;}
    }
    static final class Channel {
        int slot;String name="",identity="";boolean enabled;
        String topic(){return "c:"+slot+":"+identity;}
    }
    static final class Target {
        final String topic,title,peer,identity;final int slot;final boolean group;
        Target(String topic,String title,String peer,String identity,int slot,boolean group){this.topic=topic;this.title=title;this.peer=peer;this.identity=identity;this.slot=slot;this.group=group;}
        static Target channel(Channel c){return new Target(c.topic(),c.name,"",c.identity,c.slot,true);}
        static Target node(Node n){return new Target("d:"+(n.id.length()==64?n.id.substring(0,12):n.id),n.name.isEmpty()?n.id:n.name,n.id,"",0,false);}
    }
    static final class Message {
        String uid="",topic="",title="",sender="",body="",state="received",peer="",identity="";long timestamp;boolean outgoing,read,group;int slot;
        Target target(){return new Target(topic,title,peer,identity,slot,group);}
    }
    static final class Conversation {
        Target target;Message last;int unread;
    }
    static final class Snapshot {
        final List<Node> nodes;final List<Channel> channels;final List<Message> messages;
        Snapshot(List<Node> n,List<Channel> c,List<Message> m){nodes=Collections.unmodifiableList(n);channels=Collections.unmodifiableList(c);messages=Collections.unmodifiableList(m);}
        static Snapshot empty(){return new Snapshot(new ArrayList<>(),new ArrayList<>(),new ArrayList<>());}
        List<Conversation> conversations(){LinkedHashMap<String,Conversation> map=new LinkedHashMap<>();for(Message m:messages){Conversation c=map.get(m.topic);if(c==null){c=new Conversation();c.target=m.target();map.put(m.topic,c);}if(c.last==null||m.timestamp>=c.last.timestamp)c.last=m;if(!m.read&&!m.outgoing)c.unread++;}ArrayList<Conversation> list=new ArrayList<>(map.values());list.sort((a,b)->Long.compare(b.last.timestamp,a.last.timestamp));return list;}
        int unread(){int n=0;for(Message m:messages)if(!m.read&&!m.outgoing)n++;return n;}
    }
}
