local function format_file(filename)
  local formatter = not filename:match('%.lua$')
    and 'clang-format'
    or 'lua-format'

  local cmd = ('%s -i %s'):format(formatter, filename)
  print(cmd)
  os.execute(cmd)
end

local function run(rockspec_filename)
  local rockspec = {}
  loadfile(rockspec_filename, 't', rockspec)()
  local all_files = {}
  local function add_file(x)
    if not all_files[x] then
      all_files[x] = true
      all_files[#all_files+1] = x
    end
  end
  local function process_modules(modules)
    for _, sources in pairs(modules) do
      if type(sources) == 'string' then
        add_file(sources)
      else
        for _, x in ipairs(sources) do
          add_file(x)
        end
      end
    end
  end

  process_modules(rockspec.build.modules)

  if rockspec.build.platforms then
    for _, platform in pairs(rockspec.build.platforms) do
      process_modules(platform.modules)
    end
  end

  table.sort(all_files)
  for _, filename in ipairs(all_files) do
    format_file(filename)
  end
end

run(...)
