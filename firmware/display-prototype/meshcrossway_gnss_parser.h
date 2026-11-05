#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
namespace mcxgnss {
struct Fix {double horizontal_accuracy=0;uint32_t accuracy_ms=0,quality_ms=0;uint8_t quality=0,accuracy_source=0;bool have_accuracy=false,have_quality=false;double latitude=0,longitude=0,altitude=0,hdop=0,speed_kmh=0;unsigned satellites=0;uint64_t utc_seconds=0;uint32_t time_ms=0;bool have_time=false;uint32_t position_ms=0,received_ms=0,satellites_ms=0,speed_ms=0,frames=0,errors=0;bool valid=false,have_altitude=false,have_hdop=false,have_satellites=false,have_speed=false;};
struct Parser {
  Fix fix;char line[160]={};unsigned used=0;bool collecting=false;
  static int hex(char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;}
  static bool number(const char *s,double &out){if(!*s)return false;char *end=nullptr;out=strtod(s,&end);return end!=s&&!*end&&isfinite(out);}
  static bool coordinate(const char *s,const char *hem,bool lat,double &out){double raw;if(!number(s,raw)||raw<0||strlen(hem)!=1)return false;double deg=floor(raw/100),minutes=raw-deg*100;out=deg+minutes/60;if(minutes>=60||out>(lat?90:180))return false;if(*hem==(lat?'S':'W'))out=-out;else if(*hem!=(lat?'N':'E'))return false;return true;}
  // RMC supplies a complete UTC calendar. Reject impossible dates and leap-second
  // samples rather than normalizing bad data into a plausible system clock.
  static bool utc(const char *clock,const char *date,uint64_t &out){
    if(strlen(clock)<6||strlen(date)!=6)return false;
    for(unsigned i=0;i<6;++i)if(clock[i]<'0'||clock[i]>'9'||date[i]<'0'||date[i]>'9')return false;
    if(clock[6]){if(clock[6]!='.'||!clock[7])return false;for(unsigned i=7;clock[i];++i)if(clock[i]<'0'||clock[i]>'9')return false;}
    auto two=[](const char *p){return (p[0]-'0')*10+p[1]-'0';};
    const int hour=two(clock),minute=two(clock+2),second=two(clock+4);
    const int day=two(date),month=two(date+2),year=2000+two(date+4);
    if(year<2020||month<1||month>12||hour>23||minute>59||second>59)return false;
    static const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    auto leap=[](int y){return y%4==0&&(y%100!=0||y%400==0);};
    if(day<1||day>lengths[month-1]+(month==2&&leap(year)))return false;
    uint64_t days=0;for(int y=1970;y<year;++y)days+=leap(y)?366:365;
    for(int m=1;m<month;++m)days+=lengths[m-1]+(m==2&&leap(year));
    days+=day-1;out=days*86400+hour*3600+minute*60+second;return true;
  }
  bool parse(uint32_t now){
    char *star=strchr(line,'*');if(!star||strlen(star)!=3)return false;
    unsigned checksum=0;for(char *p=line;p<star;++p)checksum^=uint8_t(*p);
    const int a=hex(star[1]),b=hex(star[2]);if(a<0||b<0||checksum!=unsigned(a*16+b))return false;
    *star=0;char *f[24]={};unsigned n=1;f[0]=line;
    for(char *p=line;*p;++p)if(*p==','){*p=0;if(n==24)return false;f[n++]=p+1;}
    if(!strcmp(f[0],"PUBX")){
      // u-blox M10 SPG 5.10 UBX-21035062 R03 section 2.8.2.
      // Poll response only supplies accuracy; standard GGA/RMC keep owning the
      // position and clock. A PUBX reply cannot keep an old position alive.
      if(n<19||strcmp(f[1],"00"))return false;
      const bool usable=!strcmp(f[8],"G2")||!strcmp(f[8],"G3")||!strcmp(f[8],"D2")||!strcmp(f[8],"D3");
      double hacc=0;
      fix.have_accuracy=usable&&number(f[9],hacc)&&hacc>0&&hacc<=100000;
      if(fix.have_accuracy){fix.horizontal_accuracy=hacc;fix.accuracy_ms=now;fix.accuracy_source=1;}
      fix.received_ms=now;++fix.frames;return true;
    }
    if(strlen(f[0])!=5)return false;
    const char *kind=f[0]+2;double lat=0,lon=0;
    if(!strcmp(kind,"GGA")){
      if(n<11)return false;
      double quality,sats;
      if(!number(f[6],quality)||quality<0||quality>8||floor(quality)!=quality||!number(f[7],sats)||sats<0||sats>99||floor(sats)!=sats)return false;
      const bool valid=quality>0;
      if(valid&&(!coordinate(f[2],f[3],true,lat)||!coordinate(f[4],f[5],false,lon)))return false;
      fix.valid=valid;fix.quality=uint8_t(quality);fix.quality_ms=now;fix.have_quality=true;if(!valid)fix.have_accuracy=false;fix.satellites=unsigned(sats);fix.satellites_ms=now;fix.have_satellites=true;
      fix.have_hdop=number(f[8],fix.hdop)&&fix.hdop>=0;
      fix.have_altitude=number(f[9],fix.altitude)&&!strcmp(f[10],"M");
      if(valid){fix.latitude=lat;fix.longitude=lon;fix.position_ms=now;}
    }else if(!strcmp(kind,"GST")){
      if(n<9)return false;
      double latitude_sigma=0,longitude_sigma=0;
      if(!number(f[6],latitude_sigma)||!number(f[7],longitude_sigma)||latitude_sigma<0||longitude_sigma<0||latitude_sigma>100000||longitude_sigma>100000){fix.have_accuracy=false;return false;}
      const double hrms=hypot(latitude_sigma,longitude_sigma);
      fix.have_accuracy=hrms>0&&hrms<=100000;
      if(fix.have_accuracy){fix.horizontal_accuracy=hrms;fix.accuracy_ms=now;fix.accuracy_source=2;}
    }else if(!strcmp(kind,"RMC")){
      if(n<10||strlen(f[2])!=1||(*f[2]!='A'&&*f[2]!='V'))return false;
      const bool valid=*f[2]=='A';
      if(valid&&(!coordinate(f[3],f[4],true,lat)||!coordinate(f[5],f[6],false,lon)))return false;
      fix.valid=valid;if(!valid){fix.have_accuracy=fix.have_quality=false;}if(valid){fix.latitude=lat;fix.longitude=lon;fix.position_ms=now;}
      uint64_t stamp=0;
      if(valid&&utc(f[1],f[9],stamp)){fix.utc_seconds=stamp;fix.time_ms=now;fix.have_time=true;}
      double knots;fix.have_speed=valid&&number(f[7],knots)&&knots>=0;if(fix.have_speed){fix.speed_kmh=knots*1.852;fix.speed_ms=now;}
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
  Fix snapshot(uint32_t now)const{Fix out=fix;if(uint32_t(now-out.position_ms)>3000)out.valid=false;if(!out.valid||uint32_t(now-out.accuracy_ms)>3000)out.have_accuracy=false;if(!out.valid||uint32_t(now-out.quality_ms)>3000)out.have_quality=false;if(uint32_t(now-out.satellites_ms)>3000)out.have_satellites=out.have_hdop=out.have_altitude=false;if(uint32_t(now-out.speed_ms)>3000)out.have_speed=false;return out;}
};
}
