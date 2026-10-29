#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
namespace mcxgnss {
struct Fix {double latitude=0,longitude=0,altitude=0,hdop=0,speed_kmh=0;unsigned satellites=0;uint32_t position_ms=0,received_ms=0,frames=0,errors=0;bool valid=false,have_altitude=false,have_hdop=false;};
struct Parser {
  Fix fix;char line[160]={};unsigned used=0;bool collecting=false;
  static int hex(char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;}
  static bool number(const char *s,double &out){if(!*s)return false;char *end=nullptr;out=strtod(s,&end);return end!=s&&!*end&&isfinite(out);}
  static bool coordinate(const char *s,const char *hem,bool lat,double &out){double raw;if(!number(s,raw)||raw<0||strlen(hem)!=1)return false;double deg=floor(raw/100),minutes=raw-deg*100;out=deg+minutes/60;if(minutes>=60||out>(lat?90:180))return false;if(*hem==(lat?'S':'W'))out=-out;else if(*hem!=(lat?'N':'E'))return false;return true;}
  bool parse(uint32_t now){
    char *star=strchr(line,'*');if(!star||strlen(star)!=3)return false;
    unsigned checksum=0;for(char *p=line;p<star;++p)checksum^=uint8_t(*p);
    const int a=hex(star[1]),b=hex(star[2]);if(a<0||b<0||checksum!=unsigned(a*16+b))return false;
    *star=0;char *f[24]={};unsigned n=1;f[0]=line;
    for(char *p=line;*p;++p)if(*p==','){*p=0;if(n==24)return false;f[n++]=p+1;}
    if(strlen(f[0])!=5)return false;
    const char *kind=f[0]+2;double lat=0,lon=0;
    if(!strcmp(kind,"GGA")){
      if(n<11)return false;
      double quality,sats;
      if(!number(f[6],quality)||quality<0||quality>8||floor(quality)!=quality||!number(f[7],sats)||sats<0||sats>99||floor(sats)!=sats)return false;
      const bool valid=quality>0;
      if(valid&&(!coordinate(f[2],f[3],true,lat)||!coordinate(f[4],f[5],false,lon)))return false;
      fix.valid=valid;fix.satellites=unsigned(sats);
      fix.have_hdop=number(f[8],fix.hdop)&&fix.hdop>=0;
      fix.have_altitude=number(f[9],fix.altitude)&&!strcmp(f[10],"M");
      if(valid){fix.latitude=lat;fix.longitude=lon;fix.position_ms=now;}
    }else if(!strcmp(kind,"RMC")){
      if(n<10||strlen(f[2])!=1||(*f[2]!='A'&&*f[2]!='V'))return false;
      const bool valid=*f[2]=='A';
      if(valid&&(!coordinate(f[3],f[4],true,lat)||!coordinate(f[5],f[6],false,lon)))return false;
      fix.valid=valid;if(valid){fix.latitude=lat;fix.longitude=lon;fix.position_ms=now;}
      double knots;if(number(f[7],knots)&&knots>=0)fix.speed_kmh=knots*1.852;
    }
    fix.received_ms=now;++fix.frames;return true;
  }
  bool feed(char c,uint32_t now){
    if(c=='$'){used=0;collecting=true;return false;}
    if(!collecting)return false;
    if(c=='\r')return false;
    if(c=='\n'){collecting=false;line[used]=0;const bool ok=parse(now);if(!ok)++fix.errors;return ok;}
    if(used+1>=sizeof(line)){collecting=false;++fix.errors;return false;}
    line[used++]=c;return false;
  }
  Fix snapshot(uint32_t now)const{Fix out=fix;if(uint32_t(now-out.position_ms)>3000)out.valid=false;return out;}
};
}
