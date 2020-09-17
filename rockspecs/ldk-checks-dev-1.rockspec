rockspec_format = '3.0'
package = 'ldk-checks'
version = 'dev-1'
source = {
   url = 'git://github.com/dwenegar/ldk-checks.git',
}
description = {
   summary = 'Arguments Checks',
   license = 'MIT',
   maintainer = 'simone.livieri@gmail.com'
}
dependencies = {
   'lua >= 5.3'
}
build = {
   modules = {
    ['ldk.checks'] = { 'csrc/checks.c', 'csrc/liberror.c' }
   }
}
test = {
   type = 'busted'
}
