project = 'ldk-checks'
description = 'LDK - Function Arguments Checks'
dir = '../site'
file = {
  '../csrc'
}
format = 'markdown'
not_luadoc = true
no_lua_ref = true
convert_opt = true
sort = true
sort_modules = true
no_space_before_args = true
merge = false
backtick_references = false
style = '!pale'
template = '.'
custom_tags = {
  {'remark', title = 'Remarks'}
}
