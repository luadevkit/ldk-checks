/// @module ldk.checks

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <lauxlib.h>

#define CHK_EENUM (1 << 0)
#define CHK_EUSERDATA (1 << 1)
#define CHK_EFILE (1 << 2)

#define CHK_STRLEN(x) (sizeof(x) - 1)
#define CHK_STRNEQ(x, y, l) ((l) == CHK_STRLEN(x) && !strncmp(x, y, CHK_STRLEN(x)))

static void addvfstring(luaL_Buffer *b, const char *fmt, va_list argp)
{
  int n = 0;
  for (;;)
  {
    const char *e = strchr(fmt, '%');
    if (e == NULL) break;
    luaL_addlstring(b, fmt, e - fmt);
    switch (*(e + 1))
    {
      case 's':
      {
        const char *s = va_arg(argp, char *);
        luaL_addstring(b, s ? s : "(null)");
        break;
      }
      case 'd':
      {
        char digits[32];
        snprintf(digits, sizeof(digits), "%d", va_arg(argp, int));
        luaL_addstring(b, digits);
      }
    }
    n += 2;
    fmt = e + 2;
  }
  luaL_addstring(b, fmt);
}

static void addfstring(luaL_Buffer *b, const char *fmt, ...)
{
  va_list argp;
  va_start(argp, fmt);
  addvfstring(b, fmt, argp);
  va_end(argp);
}

static const char *get_name(lua_State *L, int arg)
{
  int type = luaL_getmetafield(L, arg, "__name");
  const char *name = type == LUA_TSTRING ? lua_tostring(L, -1) : NULL;
  if (type != LUA_TNIL) lua_pop(L, 1);
  return name;
}

static const char *get_type(lua_State *L, int arg, int *rawtype)
{
  int type = *rawtype = lua_type(L, arg);
  if (type == LUA_TTABLE || type == LUA_TUSERDATA)
  {
    if (type == LUA_TUSERDATA && luaL_testudata(L, arg, LUA_FILEHANDLE)) return "file";
    const char *name = get_name(L, arg);
    if (name != NULL) return name;
  }
  else if (type == LUA_TNUMBER)
  {
    if (lua_isinteger(L, arg)) return "integer";
  }
  else if (type == LUA_TSTRING)
  {
    const char *s = lua_tostring(L, arg);
    if (*s == ':') return s;
  }
  return lua_typename(L, type);
}

static int arg_error(lua_State *L, int arg, const char *message)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  lua_Debug ar;
  if (lua_getstack(L, 2, &ar))
  {
    lua_getinfo(L, "Sl", &ar);
    if (ar.currentline > 0)
    {
      addfstring(&b, "%s:%d: ", ar.short_src, ar.currentline);
    }
  }
  lua_getstack(L, 1, &ar);
  lua_getinfo(L, "n", &ar);
  addfstring(&b, "bad argument #%d to '%s'", arg, ar.name ? ar.name : "?");
  if (message)
  {
    addfstring(&b, " (%s)", message);
  }
  luaL_pushresult(&b);
  return lua_error(L);
}

static size_t add_tag(luaL_Buffer *b, const char *p, size_t len, int *flags)
{
  if (CHK_STRNEQ("userdata", p, len))
  {
    *flags |= CHK_EUSERDATA;
  }
  else if (CHK_STRNEQ("file", p, len))
  {
    *flags |= CHK_EFILE;
  }

  luaL_addlstring(b, p, len);
  return len;
}

static int add_tags(luaL_Buffer *b, const char *tags, size_t tags_len)
{
  int flags = 0;
  if (*tags == '!')
  {
    luaL_addstring(b, "not ");
    tags_len--;
    tags++;
  }
  else if (*tags == '?')
  {
    luaL_addstring(b, "nil");
    tags_len--;
    tags++;
  }
  const char *p = tags;
  while (1)
  {
    if (*p == ':') flags |= CHK_EENUM;
    const char *q = strchr(p, '|');
    if (q)
    {
      if (p != tags) luaL_addstring(b, ", ");
      tags_len -= add_tag(b, p, q - p, &flags) + 1;
      p = q + 1;
    }
    else
    {
      if (p != tags) luaL_addstring(b, ", or ");
      add_tag(b, p, tags_len, &flags);
      return flags;
    }
  }
}

static int type_error(lua_State *L, int arg, int type, const char *got, const char *expected, size_t expected_len)
{
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  int flags = add_tags(&b, expected, expected_len);
  luaL_addstring(&b, " expected, got ");
  if ((flags & CHK_EFILE) || (flags & CHK_EENUM) && *got == ':')
  {
    luaL_addstring(&b, got);
  }
  else if ((flags & CHK_EUSERDATA) && type == LUA_TUSERDATA)
  {
    luaL_addstring(&b, "userdata");
  }
  else
  {
    luaL_addstring(&b, lua_typename(L, type));
  }
  luaL_pushresult(&b);
  return arg_error(L, arg, lua_tostring(L, -1));
}

static int match_one(int rawtype, const char *got, const char *expected, size_t expected_len)
{
  if (CHK_STRNEQ("any", expected, expected_len))
  {
    return rawtype != LUA_TNONE && rawtype == LUA_TNIL;
  }
  if (*expected == ':')
  {
    return rawtype == LUA_TSTRING && *got == ':' && !strncmp(got, expected, expected_len);
  }
  if (*got == ':') return 0;
  if (!strncmp(got, expected, expected_len)) return 1;
  return rawtype == LUA_TUSERDATA && CHK_STRNEQ("userdata", expected, expected_len);
}

static int match(int type, const char *got, const char *expected, size_t expected_len)
{
  const char *q = strchr(expected, '|');
  while (q)
  {
    if (match_one(type, got, expected, q - expected)) return 1;
    expected_len -= q - expected + 1;
    expected = q + 1;
    q = strchr(expected, '|');
  }
  return match_one(type, got, expected, expected_len);
}

/***
 * Checks whether an argument of the calling function is of the given type.
 * Equivalent to luaL_checktype in the Lua C API.
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
 * or one of the following options:
 *
 * * `any` (accepts any non-nil argument type)
 * * `:id` (accepts only the exact string ':id')
 * * `file` (accepts a file object)
 * * `integer` (accepts an integer number)
 *
 * The type description can be prefixed either with a question mark, which makes it optional:
 *
 *    checktype(1, '?table') -- will match a table or nil
 *
 *  or with a exclamation mark, which makes it check against its complementary descriptor:
 *
 *    checktype(1, '!table') -- will match anything but a table
 *
 * Finally, multiple types can be accepted, if their names are concatenated with a bar `"|"`:
 *
 *    checktype(1, 'int|table') -- will match an integer or a table
 *
 * @function checktype
 * @int arg position of the argument to be tested
 * @string expected descriptor of the acceptable types
 * @usage
 *    local function foo(t, filter)
 *      checktype(1, 'table')
 *      checktype(2, 'function')
 *
 * @remark Prefixes `"?"` and `"!"` are mutually exclusive.
 */
static int check_one(lua_State *L, int arg, const char *expected, size_t expected_len)
{
  if (expected_len == 0) return 0;
  if (expected_len == 1 && *expected == '?') return 0;

  int type;
  const char *got = get_type(L, -1, &type);

  const char *p = expected;
  size_t len = expected_len;
  int ok = 1;
  if (*p == '!')
  {
    ok = 0;
    len--;
    p++;
  }
  else if (*p == '?')
  {
    if (type == LUA_TNIL)
    {
      lua_pop(L, 1);
      return 0;
    }
    len--;
    p++;
  }
  if (match(type, got, p, len) == ok) return 0;
  return type_error(L, arg, type, got, expected, expected_len);
}

static int checks_checktype(lua_State *L)
{
  lua_Debug ar;
  if (!lua_getstack(L, 1, &ar))
  {
    return luaL_error(L, "'checktype' not called from a Lua function");
  }

  int arg = (int)luaL_checkinteger(L, 1);
  size_t expected_len;
  const char *expected = luaL_checklstring(L, 2, &expected_len);

  if (!lua_getlocal(L, &ar, arg))
  {
    return luaL_argerror(L, 1, "invalid argument index");
  }
  return check_one(L, arg, expected, expected_len);
}

/**
 * Raises an error reporting a problem with argument `arg` of the function that called it,
 * using a standard message including `extramsg` as a comment.
 *
 * Equivalent to luaL_argerror in the Lua C API.
 *
 * This function never returns.
 *
 *  @function argerror
 *  @int arg argument number.
 *  @string[opt] extramsg additional text to use as comment.
 *  @usage
 *    local function foo(t, filter)
 *      argerror(1, "extra message...")
 */
static int checks_argerror(lua_State *L)
{
  int arg = (int)luaL_checkinteger(L, 1);
  const char *message = luaL_optstring(L, 2, NULL);
  return arg_error(L, arg, message);
}

// clang-format off
static const struct luaL_Reg funcs[] = {
  { "checktype", checks_checktype },
  { "argerror", checks_argerror },
  { NULL, NULL } };
//clang-format on

extern int luaopen_ldk__checks(lua_State *L)
{
  lua_newtable(L);
  luaL_setfuncs(L, funcs, 0);
  return 1;
}
