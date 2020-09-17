local function read_text(filename)
  local file <close>, err = io.open(filename, 'r')
  if err then
    error(err)
  end
  local text = file:read('a')
  return text and text:match('^%s*(.-)%s*$')
end

local function write_text(filename, text)
  local file <close> = io.open(filename, 'w+b')
  file:write(text, '\n')
end

local function fail(fmt, ...)
  io.stderr:write(fmt:format(...), '\n')
  os.exit(1)
end

local function format_version(version)
  return ('%d.%d.%d'):format(version.major, version.minor, version.patch)
end

local function parse_version(s, fail_on_error)
  local major, minor, patch = s:match('^(%d+)%.(%d+)%.(%d+)$')
  if major then
    major, minor, patch = tonumber(major), tonumber(minor), tonumber(patch)
    return {major = major, minor = minor, patch = patch }
  elseif fail_on_error then
    fail("Invalid version format: %q", s)
  end
end

local VERSION_RC = '.versionrc'

local function read_version(fail_on_error)
  local text = read_text(VERSION_RC)
  return text and parse_version(text,fail_on_error)
end

local function write_version(version)
  write_text(VERSION_RC, format_version(version))
end

--[[
  Commands
]]

local function fail_command(cmd_name)
  fail("Unknown command '%s'\nRun `%s help` for a list of available commands.", cmd_name, arg[0])
end

local Commands

local function command_new(what)
  what = what or 'patch'

  local prev_version = read_version(false)
  if not prev_version then
    prev_version = {major = 0, minor = 0, patch = 0}
  end

  local version = parse_version(what or '')
  if not version then
    version = {
      major = prev_version.major,
      minor = prev_version.minor,
      patch = prev_version.patch
    }

    if not version[what] then
      fail("Invalid argument '%s'", what)
    end

    version[what] = version[what] + 1
    if what == 'minor' then
      version.patch = 0
    elseif what == 'major' then
      version.minor = 0
      version.patch = 0
    end
  end

  if format_version(version) ~= format_version(prev_version) then
    print(format_version(prev_version) .. ' -> ' .. format_version(version))
    write_version(version)
  end
end

local function command_print()
  local version = read_version(true)
  print(format_version(version))
end

local function print_help(cmd_name, cmd)
  if cmd.arg then
    print(("Usage: %s %s %s"):format(arg[0], cmd_name, cmd.arg[1]))
    for i = 2, #cmd.arg do
      print(("       %s %s %s"):format(arg[0], cmd_name, cmd.arg[i]))
    end
  else
    print(("Usage: %s %s"):format(arg[0], cmd_name))
  end
end

local function command_help(cmd_name)
  if cmd_name then
    local cmd = Commands[cmd_name]
    if not cmd then
      fail_command(cmd_name)
    end
    print_help(cmd_name, cmd)
  else
    print(("Usage: %s [command]\n\nAvailable commands:"):format(arg[0]))
    for name, cmd in pairs(Commands) do
      print(('  %-16s%s'):format(name, cmd.description))
    end
  end
end

Commands = {
  new = {
    run = command_new,
    description = "updates the rock version.",
    arg = {
      '[minor|major|patch]',
      '[version]'
    }
  },
  print = {
    run = command_print,
    description = "prints the current rock version."
  },
  help = {
    run = command_help,
    description = "prints the application help.",
    arg = {
      '[command name]'
    }
  }
}

local function run(cmd_name, first_arg, ...)
  cmd_name = cmd_name or 'print'

  local cmd = Commands[cmd_name]
  if not cmd then
    fail_command(cmd_name)
  end

  if first_arg == 'help' then
    print_help(cmd_name, cmd)
    os.exit(0)
  end

  cmd.run(first_arg, ...)
end

run(...)
