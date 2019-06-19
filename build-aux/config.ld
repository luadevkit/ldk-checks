project = 'ldk-checks'
version = '0.2.0'
title = 'ldk-checks 0.2.0'
description = 'Function arguments type checking'
dir = '../docs'
file = {
  '../csrc',
  '../src'
}
format = 'markdown'
charset = 'utf-8'
style = '!pale'
template = '.'
no_lua_ref = true
convert_opt = true
sort = true
sort_modules = true
no_space_before_args = true
no_return_or_parms = false
not_luadoc = true
merge = true
backtick_references = false
custom_tags = {
  { 'remark', title = 'Remarks' }
}
