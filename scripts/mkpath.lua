local PATH = arg[1]:lower()
local drive, path = PATH:match('^(%a):(.-)$')
if not drive then
  path = PATH
end
path = path:match('^/?(.*)')
if drive then
  print(('/%s/%s'):format(drive, path))
else
  print(('/%s'):format(path))
end
