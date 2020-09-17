/***
 * Function arguments checking.
 * @module ldk.checks
 */

#include "liberror.h"

#include <assert.h>
#include <ctype.h>
#include <lauxlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define str_len(x) (sizeof(x) - 1)
#define str_leq(x, xl, y, yl) ((yl) == (xl) && strncmp(x, y, xl) == 0)
#define str_eq(x, xl, y) ((xl) == str_len(y) && strncmp(x, y, xl) == 0)

static int checkers_ref = LUA_NOREF;

static const char *get_meta_field(lua_State *L, int arg, const char *field_name, size_t *len)
{
    int field_type = luaL_getmetafield(L, arg, field_name);
    if (field_type == LUA_TNIL) return NULL;

    const char *value;
    if (field_type == LUA_TSTRING)
    {
        value = lua_tolstring(L, -1, len);
    }
    lua_pop(L, 1);
    return value;
}

// must be called with the value to check at the top of the stack
static const char *get_specific_type(lua_State *L, int type, size_t *len)
{
    if (type == LUA_TNUMBER)
    {
        if (lua_isinteger(L, -1))
        {
            *len = str_len("integer");
            return "integer";
        }
        *len = str_len("float");
        return "float";
    }

    const char *name = NULL;
    if (type == LUA_TUSERDATA)
    {
        name = get_meta_field(L, -1, "__name", len);
    }
    else if (type == LUA_TTABLE)
    {
        name = get_meta_field(L, -1, "__type", len);
    }
    if (name == NULL)
    {
        name = lua_typename(L, type);
        *len = strlen(name);
    }
    return name;
}

static inline void append_descriptor0(luaL_Buffer *b, const char *p, size_t len, bool is_option)
{
    if (is_option)
    {
        luaL_addchar(b, '\'');
        luaL_addlstring(b, p, len);
        luaL_addchar(b, '\'');
    }
    else if (str_eq(p, len, "any"))
    {
        luaL_addstring(b, "anything but nil");
    }
    else
    {
        luaL_addlstring(b, p, len);
    }
}

static void append_descriptor(luaL_Buffer *b, const char *descriptor, size_t descriptor_len, bool is_option)
{
    const char *p = descriptor;
    const char *e = descriptor + descriptor_len;

    if (!is_option)
    {
        if (*p == '*')
        {
            luaL_addstring(b, "zero or more of ");
            p++;
        }
        else if (*p == '+')
        {
            luaL_addstring(b, "one or more of ");
            p++;
        }
    }

    int n = 0;
    if (*p == '?')
    {
        luaL_addstring(b, "nil");
        p++;
        n++;
    }
    for (const char *q = strchr(p, '|'); q != NULL; p = q + 1, q = strchr(p, '|'), n++)
    {
        if (n > 0) luaL_addstring(b, ", ");
        append_descriptor0(b, p, (size_t)(q - p), is_option);
    }
    if (n > 0) luaL_addstring(b, n > 2 ? ", or " : " or ");
    append_descriptor0(b, p, (size_t)(e - p), is_option);
}

static void push_type_error(lua_State *L, int type, const char *expected, size_t expected_len)
{
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    append_descriptor(&b, expected, expected_len, false);
    luaL_addstring(&b, " expected, got ");
    luaL_addstring(&b, lua_typename(L, type));
    luaL_pushresult(&b);
}

static void push_option_error(lua_State *L,                    //
                              const char *got, size_t got_len, //
                              const char *expected, size_t expected_len)
{
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    append_descriptor(&b, expected, expected_len, true);
    luaL_addstring(&b, " expected, got '");
    luaL_addlstring(&b, got, got_len);
    luaL_addchar(&b, '\'');
    luaL_pushresult(&b);
}

static bool options_match(lua_State *L,                    //
                          const char *got, size_t got_len, //
                          const char *expected, const char *expected_end)
{
    const char *p = expected;
    const char *q = p;
    while (p < expected_end)
    {
        q = (const char *)memchr(p, '|', (size_t)(expected_end - q));
        if (q == NULL) q = expected_end;
        if (str_leq(got, got_len, p, (size_t)(q - p))) return true;
        p = q + 1;
    }
    return false;
}

// must be called with the value to check at the top of the stack
// the value is popped at the end
static int options_check_one(lua_State *L, int level, int arg, const char *expected, size_t expected_len)
{
    // val
    int type = lua_type(L, -1); // val

    const char *p = expected;
    const char *e = expected + expected_len;
    if (*p == '?')
    {
        if (type == LUA_TNIL) return 1;
        if (++p == e) return 0;
    }

    if (type != LUA_TSTRING)                                   // val
    {                                                          //
        lua_pop(L, 1);                                         //
        push_type_error(L, type, "string", str_len("string")); // error
        return errorL_argerror(L, level, arg, lua_tostring(L, -1));
    }

    size_t got_len;
    const char *got = lua_tolstring(L, -1, &got_len);
    if (options_match(L, got, got_len, p, e))
    {
        lua_pop(L, 1);
        return 1;
    }

    push_option_error(L, got, got_len, expected, expected_len); // val error
    lua_replace(L, -2);                                         // error
    return errorL_argerror(L, level, arg, lua_tostring(L, -1));
}

/**
 * Checks whether an argument of the calling function is a string and its value is
 * any of the values specified in a given descriptor.
 *
 * Multiple values can be accepted if concatenated with a bar `|`:
 *
 *    check_option(1, 'one|two') -- accepts both `one` and `two`.
 *
 * A `nil` value can be accepted by prefixing the value descriptor with a
 * question mark:
 *
 *    check_option(1, '?one') -- matches 'one' or nil
 *
 * @function check_option
 * @tparam integer arg the position of the argument to be tested.
 * @tparam string expected the acceptable values descriptor.
 * @usage
 *    local function foo(mode)
 *      check_option(1, 'read|write')
 *      ...
 * @raise If the descriptor is `nil` or empty; or if the argument position is invalid.
 */
static int checks_check_option(lua_State *L)
{
    int arg = (int)luaL_checkinteger(L, 1);
    size_t expected_len;
    const char *expected = luaL_checklstring(L, 2, &expected_len);
    if (expected_len == 0)
    {
        return luaL_argerror(L, 2, "empty descriptor");
    }
    int level = (int)luaL_optinteger(L, 3, 1);

    lua_Debug ar;
    lua_getstack(L, 1, &ar);
    if (!lua_getlocal(L, &ar, arg))
    {
        return luaL_argerror(L, 1, "invalid argument index");
    }

    // val
    if (options_check_one(L, level, arg, expected, expected_len)) return 0;
    return luaL_argerror(L, 2, "invalid descriptor");
}

static bool type_match_one_fast(int type, const char *expected, size_t expected_len, bool *is_match)
{
    if (type != LUA_TNIL && str_eq(expected, expected_len, "any"))
    {
        *is_match = true;
        return true;
    }

    if (type == LUA_TNUMBER && str_eq(expected, expected_len, "number"))
    {
        *is_match = true;
        return true;
    }

    switch (type)
    {
        case LUA_TBOOLEAN:
            *is_match = str_eq(expected, expected_len, "boolean");
            return true;
        case LUA_TSTRING:
            *is_match = str_eq(expected, expected_len, "string");
            return true;
        case LUA_TFUNCTION:
            *is_match = str_eq(expected, expected_len, "function");
            return true;
        case LUA_TTHREAD:
            *is_match = str_eq(expected, expected_len, "thread");
            return true;
    }

    if (type == LUA_TUSERDATA || type == LUA_TLIGHTUSERDATA)
    {
        if (str_eq(expected, expected_len, "userdata"))
        {
            *is_match = true;
            return true;
        }
    }
    return false;
}

static bool type_match_one_slow(lua_State *L, int type,          //
                                const char *got, size_t got_len, //
                                const char *expected, size_t expected_len)
{
    if (str_leq(got, got_len, expected, expected_len)) return true;

    if (type == LUA_TUSERDATA && str_eq(expected, expected_len, "file"))
    {
        return str_leq(LUA_FILEHANDLE, str_len(LUA_FILEHANDLE), got, got_len);
    }

    if (checkers_ref == LUA_NOREF) return false;

    lua_pushlstring(L, expected, expected_len); // val checkers expected
    if (!lua_gettable(L, -2))
    {
        lua_pop(L, 1); // val checkers
    }
    else // val checkers checker
    {
        if (lua_isfunction(L, -1))
        {
            lua_pushvalue(L, -3); // val checkers checker val
            lua_call(L, 1, 1);    // val checkers result
            if (lua_toboolean(L, -1))
            {
                lua_pop(L, 1); // val checkers
                return true;
            }
        }
        lua_pop(L, 1); // val checkers
    }
    return false;
}

// must be called with the value to check at the top of the stack
// the value is popped at the end
static bool type_match(lua_State *L, int type, const char *expected, const char *expected_end)
{
    // value
    size_t got_len;
    const char *got = NULL;

    const char *p = expected;
    const char *q = p;
    while (p < expected_end)
    {
        q = (const char *)memchr(p, '|', (size_t)(expected_end - q));
        if (q == NULL) q = expected_end;

        bool is_match;
        if (type_match_one_fast(type, p, (size_t)(q - p), &is_match))
        {
            lua_pop(L, 1);
            return is_match;
        }

        if (got == NULL)
        {
            got = get_specific_type(L, type, &got_len);
            if (checkers_ref != LUA_NOREF)
            {
                lua_geti(L, LUA_REGISTRYINDEX, checkers_ref); // checkers
            }
        }

        if (type_match_one_slow(L, type, got, got_len, p, (size_t)(q - p)))
        {
            lua_pop(L, checkers_ref == LUA_NOREF ? 1 : 2);
            return true;
        }
        p = q + 1;
    }

    lua_pop(L, checkers_ref == LUA_NOREF ? 1 : 2);
    return false;
}

// must be called with the value to check at the top of the stack
// the value is popped at the end
static int type_check_one(lua_State *L, int level, int arg, const char *expected, size_t expected_len)
{
    // val
    if (*expected == ':')
    {
        if (--expected_len == 0) return 0;
        return options_check_one(L, level, arg, ++expected, expected_len);
    }

    int type = lua_type(L, -1);

    const char *p = expected;
    const char *e = expected + expected_len;
    if (*p == '?')
    {
        if (type == LUA_TNIL) return 1;
        if (++p == e) return 0;
    }

    if (type_match(L, type, p, e)) return 1; // <empty>
    push_type_error(L, type, expected, expected_len);
    return errorL_argerror(L, level, arg, lua_tostring(L, -1));
}

/***
 * Checks whether the specified argument of the calling function is of any of the types specified in
 * the given descriptor.
 *
 * The type of the argument at position `arg` must match one of the types in `expected.
 *
 * Each type can be the name of a primitive Lua type:
 *
 * * `nil`
 * * `boolean`
 * * `userdata`
 * * `number`
 * * `string`
 * * `table`
 * * `function`
 * * `thread`
 *
 * one of the following options:
 *
 * * `any` (accepts any non-nil argument type)
 * * `file` (accepts a file object)
 * * `integer` (accepts an integer number)
 * * `float` (accepts a floating point number)
 * * an arbitrary string, matched against the content of the `__type` or `__name` field of the
 * argument's metatable if the argument is table or a userdata, respectively.
 *
 * Multiple values can be accepted if concatenated with a bar `|`:
 *
 *    check_type(1, 'integer|table') -- matches an integer or a table
 *
 * A `nil` value can be matched by prefixing the type descriptor with `?`:
 *
 *     checktype(1, '?table') -- matches a table or nil
 *
 * Finally, if a descriptor is prefixed with `:`, the function will behave like @{check_option}.
 *
 *     checktype(1, ':one|two') -- matches 'one' or 'two'
 *
 * @remark Prefixes are processed in order: first `:` then `?`.
 * @function check_type
 * @tparam integer arg position of the argument to be tested.
 * @tparam string expected the descriptor of the expected type.
 * @tparam[opt=1] integer level the level in the call stack at which to report the error.
 * @usage
 *    local function foo(t, filter)
 *      check_type(1, 'table')
 *      check_type(2, '?function')
 *      ...
 */
static int checks_check_type(lua_State *L)
{
    int arg = (int)luaL_checkinteger(L, 1);

    size_t expected_len;
    const char *expected = luaL_checklstring(L, 2, &expected_len);
    if (expected_len == 0)
    {
        return luaL_argerror(L, 2, "empty descriptor");
    }
    int level = (int)luaL_optinteger(L, 3, 1);

    lua_Debug ar;
    lua_getstack(L, 1, &ar);
    if (!lua_getlocal(L, &ar, arg))
    {
        return luaL_argerror(L, 1, "invalid argument index");
    }

    // val
    if (type_check_one(L, level, arg, expected, expected_len)) return 0;
    return luaL_argerror(L, 2, "invalid descriptor");
}

/***
 * Checks whether the arguments of the calling function are of the specified types.
 *
 * The last descriptor can be prefixed with:
 *
 * -  a `*` to make it match zero more more arguments.
 * -  a `+` to make it match one more more arguments.
 *
 * @function check_types
 * @tparam string ... the descriptors of the expected types (see @{check_type}).
 * @tparam[opt=1] integer level the level in the call stack at which to report the error.
 * @usage
 *    local function foo(t, filter)
 *      check_types('table', '?function')
 *      ...
 */
static int checks_check_types(lua_State *L)
{
    int n = lua_gettop(L);

    int level = 1;
    if (lua_isinteger(L, -1))
    {
        level = lua_tointeger(L, -1);
        n--;
    }

    lua_Debug ar;
    lua_getstack(L, 1, &ar);

    char eat_all = 0;
    size_t expected_len;
    const char *expected;

    const char *p, *e;
    int arg = 1;
    while (arg <= n)
    {
        expected = luaL_checklstring(L, arg, &expected_len);
        if (expected_len == 0)
        {
            return luaL_argerror(L, arg, "empty descriptor");
        }

        p = expected;
        e = expected + expected_len;
        if (*p == '*' || *p == '+')
        {
            eat_all = *p++;
            if (p == e)
            {
                return luaL_argerror(L, arg, "invalid descriptor");
            }
            break;
        }

        if (!lua_getlocal(L, &ar, arg))
        {
            push_type_error(L, LUA_TNONE, expected, expected_len);
            return errorL_argerror(L, level, arg, lua_tostring(L, -1));
        }

        // val
        type_check_one(L, level, arg, p, (size_t)(e - p));
        arg++;
    }

    if (!eat_all) return 0;

    int arg_count = arg;
    while (lua_getlocal(L, &ar, arg)) // checkers val
    {
        type_check_one(L, level, arg++, p, (size_t)(e - p));
    }

    int vararg = -1;
    while (lua_getlocal(L, &ar, vararg--)) // checkers val
    {
        type_check_one(L, level, arg++, p, (size_t)(e - p));
    }

    if (arg > arg_count || eat_all == '*') return 0;
    push_type_error(L, LUA_TNONE, expected, expected_len);
    return errorL_argerror(L, level, arg, lua_tostring(L, -1));
}

/**
 * Raises an error reporting a problem with the argument of the calling function at the specified
 * position.
 *
 * This function never returns.
 *
 * @function arg_error
 * @tparam integer arg the argument position.
 * @tparam[opt] string message additional text to use as comment.
 * @tparam[opt=1] integer level the level in the call stack at which to report the error.
 * @usage
 *    local function foo(x)
 *      arg_error(1, "message...")
 *      ...
 */
static int checks_arg_error(lua_State *L)
{
    int arg = (int)luaL_checkinteger(L, 1);
    const char *extramsg = luaL_optstring(L, 2, NULL);
    int level = (int)luaL_optinteger(L, 3, 1);
    if (level > 0)
    {
        errorL_argerror(L, level, arg, extramsg);
    }
    return 0;
}

/**
 * Raises an error reporting a problem with the type of the argument of the calling function at the specified
 * position.
 *
 * This function never returns.
 *
 * @function type_error
 * @tparam integer arg the argument position.
 * @tparam string expected the expected type.
 * @tparam[opt=1] integer level the level in the call stack at which to report the error.
 * @usage
 *    local function foo(x)
 *      type_error(1, "integer")
 *      ...
 */
static int checks_type_error(lua_State *L)
{
    int arg = (int)luaL_checkinteger(L, 1);
    size_t expected_len;
    const char *expected = luaL_checklstring(L, 2, &expected_len);
    int level = (int)luaL_optinteger(L, 3, 1);
    if (level > 0)
    {
        lua_Debug ar;
        lua_getstack(L, 1, &ar);
        if (!lua_getlocal(L, &ar, arg))
        {
            return luaL_argerror(L, 1, "invalid argument index");
        }

        int type = lua_type(L, arg);
        push_type_error(L, type, expected, expected_len);
    }
    return 0;
}

/**
 * Raises an error reporting a problem with the argument of the calling function at the specified
 * position if the given condition is not satisfied.
 *
 * @function check_arg
 * @tparam integer arg the position of the argument to be tested.
 * @tparam bool condition condition to check.
 * @tparam[opt] string message additional text to use as comment.
 * @tparam[opt=1] integer level the level in the call stack at which to report the error.
 * @usage
 *    local function foo(t, filter)
 *      check_type(1, 'table')
 *      check_arg(1, #t > 0, "empty table")
 *      ...
 */
static int checks_check_arg(lua_State *L)
{
    int arg = (int)luaL_checkinteger(L, 1);
    int cond = lua_toboolean(L, 2);
    const char *extramsg = luaL_optstring(L, 3, NULL);
    int level = (int)luaL_optinteger(L, 4, 1);
    if (level > 0 && !cond)
    {
        errorL_argerror(L, level, arg, extramsg);
    }
    return 0;
}

/**
 * Register a custom check function for a given type descriptor.
 *
 * Passing `nil` as the custom check function will unregister the custom check.
 *
 * @function register
 * @tparam string descriptor the type descriptor to register a check function for.
 * @tparam function check the custom check function.
 */
static int checks_register(lua_State *L)
{
    size_t descriptor_len;
    const char *descriptor = luaL_checklstring(L, 1, &descriptor_len);

    if (descriptor_len == 0)
    {
        return luaL_argerror(L, 1, "name is empty");
    }

    if (checkers_ref == LUA_NOREF)
    {
        lua_newtable(L);                               // checkers
        checkers_ref = luaL_ref(L, LUA_REGISTRYINDEX); //
    }

    if (lua_isnil(L, 2))
    {
        lua_geti(L, LUA_REGISTRYINDEX, checkers_ref); // checkers
        lua_pushnil(L);                               // checkers nil
        lua_setfield(L, -2, descriptor);              // checkers
        lua_pop(L, 1);
        return 0;
    }

    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_geti(L, LUA_REGISTRYINDEX, checkers_ref); // checkers
    lua_pushvalue(L, 2);                          // checkers function
    lua_setfield(L, -2, descriptor);              // checkers
    lua_pop(L, 1);
    return 0;
}

// clang-format off
static const struct luaL_Reg funcs[] =
{
#define XX(name) { #name, checks_ ##name },
    XX(arg_error)
    XX(check_arg)
    XX(check_option)
    XX(check_type)
    XX(check_types)
    XX(register)
    { NULL, NULL }
#undef XX
};
//clang-format on

extern int luaopen_ldk_checks(lua_State *L)
{
    luaL_newlib(L, funcs);
    return 1;
}
