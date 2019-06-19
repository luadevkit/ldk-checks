rockspec_format = '3.0'

package = 'ldk-checks'
version = 'dev-1'
source = {
  url = 'git://github.com/luadevkit/ldk-checks.git',
  branch = 'dev'
}
description = {
  summary = 'LDK - function arguments type checking',
  license = 'MIT',
  maintainer = 'info@luadevk.it'
}
dependencies = {
  'lua >= 5.3'
}
build = {
  modules = {
    ['ldk._checks'] = 'csrc/_checks.c',
    ['ldk.checks'] = 'src/ldk/checks.lua'
  }
}
