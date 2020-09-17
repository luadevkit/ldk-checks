#include "liberror.h"

#include <lauxlib.h>
#include <string.h>

static int findfield(lua_State *L, int object_index, int level)
{
    if (level == 0 || !lua_istable(L, -1)) return 0;
    lua_pushnil(L);                                    // nil
    while (lua_next(L, -2))                            // k, v
    {                                                  //
        if (lua_type(L, -2) == LUA_TSTRING)            //
        {                                              //
            if (lua_rawequal(L, object_index, -1))     //
            {                                          //
                lua_pop(L, 1);                         // k
                return 1;                              //
            }                                          //
            if (findfield(L, object_index, level - 1)) // k v k
            {                                          //
                lua_pushliteral(L, ".");               // k v k "."
                lua_replace(L, -3);                    // k "." k
                lua_concat(L, 3);                      // "k.k"
                return 1;                              //
            }                                          //
        }                                              //
        lua_pop(L, 1);                                 // k
    }
    return 0;
}

static int pushglobalfuncname(lua_State *L, lua_Debug *ar)
{
    int top = lua_gettop(L);

    lua_getinfo(L, "f", ar);                              // f
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE); // f LUA_LOADED_TABLE
                                                          //
    if (findfield(L, top + 1, 2))                         // f LUA_LOADED_TABLE name
    {                                                     //
        const char *name = lua_tostring(L, -1);           // f LUA_LOADED_TABLE name
        if (strncmp(name, "_G.", 3) == 0)                 //
        {                                                 //
            lua_pushstring(L, name + 3);                  // f LUA_LOADED_TABLE name name + 3
            lua_remove(L, -2);                            // f LUA_LOADED_TABLE name + 3
        }                                                 //
        lua_copy(L, -1, top + 1);                         // name f LUA_LOADED_TABLE
        lua_settop(L, top + 1);                           // name
        return 1;
    }

    lua_settop(L, top);
    return 0;
}

int errorL_errorf(lua_State *L, int level, const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    luaL_where(L, level + 1);
    lua_pushvfstring(L, fmt, argp);
    va_end(argp);
    lua_concat(L, 2);
    return lua_error(L);
}

int errorL_argerror(lua_State *L, int level, int arg, const char *extramsg)
{
    lua_Debug ar;
    if (!lua_getstack(L, level, &ar))
    {
        return errorL_errorf(L, level, "bad argument #%d (%s)", arg, extramsg);
    }
    lua_getinfo(L, "n", &ar);
    if (strcmp(ar.namewhat, "method") == 0 && --arg == 0)
    {
        return extramsg ? errorL_errorf(L, level, "calling '%s' on bad self (%s)", ar.name, extramsg)
                        : errorL_errorf(L, level, "calling '%s' on bad self", ar.name);
    }
    if (ar.name == NULL)
    {
        ar.name = pushglobalfuncname(L, &ar) ? lua_tostring(L, -1) : "?";
    }
    return extramsg ? errorL_errorf(L, level, "bad argument #%d to '%s' (%s)", arg, ar.name, extramsg)
                    : errorL_errorf(L, level, "bad argument #%d to '%s'", arg, ar.name);
}
