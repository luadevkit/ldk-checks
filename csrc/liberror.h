#pragma once

#include <lua.h>

int errorL_errorf(lua_State *L, int level, const char *fmt, ...);
int errorL_argerror(lua_State *L, int level, int arg, const char *extramsg);
