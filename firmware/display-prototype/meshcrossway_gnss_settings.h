#pragma once
#include <stdint.h>
namespace mcxgnss {
struct Settings {uint32_t version=1,baud=0,timeout_ms=3000;uint8_t enabled=1,imperial=0;uint8_t reserved[2]={}; /* Stable manual timezone ID; zero = GPS auto. */};
static bool valid_settings(const Settings &s){return s.version==1&&(s.baud==0||s.baud==9600||s.baud==38400||s.baud==115200)&&s.enabled<=1&&s.imperial<=1&&(s.timeout_ms==3000||s.timeout_ms==5000||s.timeout_ms==10000);}
}
