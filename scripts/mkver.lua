local VERSIONRC = '.versionrc'

local function format(msg, x, ...)
  if x then
    msg = msg:format(x, ...)
  end
  return msg
end

local function log(...)
  local msg = format(...)
  msg = ('> %s\n'):format(msg)
  io.stdout:write(msg)
end

local function fail(...)
  local msg = format(...)
  msg = ('%s: %s\n'):format(arg[0], msg)
  io.stderr:write(msg)
  os.exit(1)
end

local BoolOpt = function()
  return 0, true
end

local function parse_opts(args, opts)
  local a, o = {}, {}
  local i = 1
  while i <= #args do
    local arg = args[i]
    local opt_name = arg:match('^%-%-(%S+)$')
    if opt_name then
      local opt_f = opts[opt_name]
      if not opt_f then
        fail("unknown option '%s'", arg)
      end
      local d, v = opt_f(args, i)
      if not v then
        fail("missing argument to option '%s'", arg)
      end
      o[opt_name] = v
      i = i + d
    else
      a[#a + 1] = arg
    end
    i = i + 1
  end
  return a, o
end

local function mk_string(version)
  return ('%d.%d.%d'):format(version.major, version.minor, version.patch)
end

local function parse_tag(tag)
  local major, minor, patch = tag:match('^(%d+)%.(%d+)%.(%d+)')
  if major then
    major, minor, patch = tonumber(major), tonumber(minor), tonumber(patch)
    if not major or not minor or not patch then
      error('invalid tag: ' .. tag)
    end
    return {
      major = major,
      minor = minor,
      patch = patch
    }
  end
end

local function read_versionrc()
  local f = io.open(VERSIONRC, 'r')
  local text = f:read('a')
  f:close()
  return parse_tag(text)
end

local function write_versionrc(version)
  version = mk_string(version)
  local f = io.open(VERSIONRC, 'w')
  f:write(version)
  f:close()
  log("wrote '%s' to %s", version, VERSIONRC)
end

local function new_version(...)
  local args, opts = parse_opts(table.pack(...), {
    tag = BoolOpt
  })

  if args[1] then
    local version = parse_tag(args[1])
    if version then
      return write_versionrc(version)
    end
  end

  local version = read_versionrc()
  if not version then
    version = { major = 0, minor = 1, patch = 0 }
  end

  local what = args[1] or 'patch'
  local value = version[what]
  if not value then
    fail("invalid argument '%s'", what)
  end

  version[what] = value + 1
  write_versionrc(version)

  if opts.tag then
    local git_cmd = 'git tag '
    git_cmd = git_cmd .. make_tag(version)
    log(git_cmd)
    os.execute(git_cmd)
  end
end

local function print_version()
  local version = read_versionrc()
  if not version then
    fail("initialize the project first")
  end
  print(mk_string(version))
end

local function tag_version(...)
  local _, opts = parse_opts(table.pack(...), {
    force = BoolOpt
  })

  local version = read_versionrc()
  if not version then
    fail("initialize the project first")
  end

  local git_cmd = 'git tag '
  if opts.force then
    git_cmd = git_cmd .. '-f '
  end
  git_cmd = git_cmd .. mk_string(version)
  log(git_cmd)
  os.execute(git_cmd)
end

local function init(version)
  version = version and parse_tag(version)
  if not version then
    version = { major = 0, minor = 1, patch = 0 }
  end
  write_versionrc(version)
end

local function list_tags()
  local f = io.popen('git tag')
  for line in f:lines() do
    local tag = line:match('^%d+%.%d+%.%d+$')
    if tag then
      print(tag)
    end
  end
  f:close()
end

local commands = {
  print = print_version,
  new = new_version,
  tag = tag_version,
  tags = list_tags,
  init = init
}

local function run_cmd(name, ...)
  name = name or 'print'
  local cmd = commands[name]
  if cmd then
    cmd(...)
  else
    fail("unknown command '%s'", name)
  end
end

run_cmd(table.unpack(arg))
