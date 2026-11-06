// SPDX-License-Identifier: MIT
// Host-only smoke tests: g++ -std=c++17 -Wall -Wextra -pedantic tests/ofm_tile_plan_test.cpp -o /tmp/ofm_tile_plan_test && /tmp/ofm_tile_plan_test
#include "../firmware/display-prototype/meshcrossway_ofm_tile_plan.h"
#include <cassert>
int main() {
  using namespace mcx_ofm_plan;
  assert(tile_x(0,0)==0 && tile_y(0,0)==0);
  assert(tile_x(0,1)==1 && tile_y(0,1)==1);
  assert(tile_x(180,2)==0); // longitude wraps
  assert(count(rect(-85,-180,85,180,0))==1);
  assert(count(rect(-85,-180,85,180,1))==4);
  assert(rect(-1,179,1,180,2).right==3);
  assert(rect(-1,-180,1,-179,2).left==0);
  assert(tile_y(90,2)==0 && tile_y(-90,2)==3);
  assert(count(rect(-1,-1,1,1,0))==1);
  assert(!rect(-1,170,1,-170,3).valid); // antimeridian explicit rejection
  assert(!rect(-1,-181,1,1,3).valid);
  assert(!rect(1,0,-1,1,3).valid);
  assert(!rect(-1,-1,1,1,23).valid);
  assert(evaluate(3000,50000000ULL).accepted);
  assert(!evaluate(3001,1).accepted);
  assert(!evaluate(1,50000001ULL).accepted);
  assert(estimate(-0.01,-0.01,0.01,0.01,0,0,100).accepted);
  assert(!estimate(-80,-170,80,170,0,14,100).accepted);
  return 0;
}
