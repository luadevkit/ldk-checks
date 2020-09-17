local IS_WINDOWS = package.config:sub(1, 1) == '\\'

local function run(path)
  print("Creating " .. path)
  local cmd = IS_WINDOWS
    and 'cmd /c mkdir %s'
    or 'mkdir -p %s'
  if IS_WINDOWS then
    path = path:gsub('/', '\\')
  end
  os.execute(cmd:format(path))
end

run(...)
