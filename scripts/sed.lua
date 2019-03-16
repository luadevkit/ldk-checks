local input, output = arg[1], arg[2]

local file, err = io.open(input, 'r')
if err then
  error(err)
end
local text = file:read('a')
file:close()

local repl = {}
for i = 3, #arg do
  local kvp = arg[i]
  local key, value = kvp:match('^([%a_]+)=(.*)$')
  if not key then
    error('invalid key-value pair: ' .. kvp)
  end
  repl[key] = value
end

file, err = io.open(output, 'w')
if err then
  error(err)
end
file:write((text:gsub('@([%a_]+)@', repl)))
file:close()
