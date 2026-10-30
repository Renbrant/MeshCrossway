#!/usr/bin/env python3
"""Exercise actual remote mailbox, parsers and both production protocol send models."""
from pathlib import Path
import subprocess,tempfile,json
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory)
 for source,target in [('meshcrossway_remote_chat.h','remote_model.h'),('meshcrossway_remote_chat_http.h','remote_http.h')]:
  content=(ROOT/'firmware/display-prototype'/source).read_text()
  content='\n'.join(line for line in content.splitlines() if not line.startswith('#include "'))
  (p/target).write_text(content)
 binary=p/'remote'
 subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wno-unused-function','-Werror=misleading-indentation','-fsanitize=address,undefined','-I'+str(p),'-I'+str(ROOT/'firmware/display-prototype'),str(ROOT/'tests/remote_chat_test.cpp'),'-lcrypto','-o',str(binary)],check=True)
 output=subprocess.check_output([str(binary)],text=True,env={**__import__('os').environ,'ASAN_OPTIONS':'detect_leaks=0'})
 rows=[json.loads(line) for line in output.splitlines()]
 assert rows[0]['network']=='mc' and rows[2]['network']=='mt'
 assert rows[-1]['older']==56 and len(rows[-1]['messages'])==40
 print('PASS: real MC/MT send boundaries, channel/radio identity, offline/blocked/oversize rejection, UTF-8/JSON, paging, bounded PSRAM, HTTP validation and timeout cancellation')

 # Compile the actual radio-control handlers with a deterministic BLE adapter.
 source=(ROOT/'tests/remote_chat_test.cpp').read_text().split('using namespace mcxremotechat;')[0]
 source=source.replace('static void vTaskDelay(unsigned){if(owner_poll)mcxremotechat::poll();}', 'static void (*owner_extra)()=nullptr;static void vTaskDelay(unsigned){if(owner_poll){mcxremotechat::poll();if(owner_extra)owner_extra();}}')
 radio=(ROOT/'firmware/display-prototype/meshcrossway_remote_radio.h').read_text()
 (p/'remote_radio.h').write_text('\n'.join(line for line in radio.splitlines() if not line.startswith('#include "')))
 http=(ROOT/'firmware/display-prototype/meshcrossway_navigation_http.h').read_text()
 quote=http[http.index('  static std::string quote('):http.index('  static esp_err_t fail(')]
 number=http[http.index('  static bool number('):http.index('  // A required non-simple header')]
 cpp=p/'radio.cpp';cpp.write_text(source+(ROOT/'tests/remote_radio_test.cpp').read_text().replace('// PRODUCTION_HTTP_HELPERS',quote+number))
 binary=p/'radio'
 subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wno-unused-function','-Werror=misleading-indentation','-fsanitize=address,undefined','-I'+str(p),'-I'+str(ROOT/'firmware/display-prototype'),str(cpp),'-lcrypto','-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True,env={**__import__('os').environ,'ASAN_OPTIONS':'detect_leaks=0'})
