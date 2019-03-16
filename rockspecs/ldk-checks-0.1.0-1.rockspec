rockspec_format = '3.0'

package = 'ldk-checks'
version = '0.1.0-1'
source = {
  url = 'https://github.com/luadevkit/ldk-checks.git'
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
    ['ldk.checks'] = 'csrc/checks.c'
  }
}
