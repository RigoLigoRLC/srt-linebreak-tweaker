
#pragma once

#include <rint.h>

class QString;

u64 TCtoMS(int h, int m, int s, int ms);
QString MStoTC(u64 ms);
QString MStoSrtTC(u64 ms);
inline i32 I32Max2(i32 a, i32 b) { return a > b ? a : b; }
inline i32 I32Min2(i32 a, i32 b) { return a < b ? a : b; }
inline i32 Sign(i32 x) { return x > 0 ? 1 : -1 ; }
inline i32 BoundTo(i32 x, i32 lb, i32 ub) { if(x > ub) return ub; if(x < lb) return lb; return x; }
