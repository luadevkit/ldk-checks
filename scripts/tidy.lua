local function open_file(filename, mode)
  local file, err = io.open(filename, mode)
  if err then
    error(err)
  end

  return file
end

local function write_lines(filename, lines)
  local file = open_file(filename, 'w+b')
  for _, line in ipairs(lines) do
    local _, err = file:write(line, '\n')
    if err then
      error(err)
    end
  end

  file:close()
end

local function tidy_markdown(filename, lines)
  local after_empty_line = true
  local function process_line(line)
    if #line == 0 and after_empty_line then
      return
    end

    local is_header = not not line:match('#')
    if is_header and not after_empty_line then
      lines[#lines  + 1] = ''
    end

    lines[#lines  + 1] = line
    if is_header then
      lines[#lines  + 1] = ''
    end

    after_empty_line = #line == 0 or is_header or not not line:match('<a')
  end

  for line in io.lines(filename) do
    process_line(line:match('^%s*(.-)%s*$'))
  end
end

local function tidy_file(filename, lines)
  for line in io.lines(filename) do
    line = line:match('^(.-)%s*$')
    if #line > 0 then
      lines[#lines + 1] = line
    end
  end
end

local function run(...)
  local filenames = table.pack(...)
  for _, filename in ipairs(filenames) do
    local lines = {}
    if filename:match('%.md$') then
      tidy_markdown(filename, lines)
    else
      tidy_file(filename, lines)
    end

    if #lines[#lines] == 0 then
      lines[#lines] = nil
    end

    write_lines(filename, lines)
  end
end

run(...)
