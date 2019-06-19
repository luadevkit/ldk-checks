--- @module ldk.checks
local M = {}

local _checks = require 'ldk._checks'

local _ENV = M

checktype = _checks.checktype
argerror = _checks.argerror

--- Raises an error reporting a problem with argument `arg` of the function that called it,
-- using a standard message including a formatted message as a comment.
-- This function never returns.
-- @tparam integer arg argument number.
-- @tparam string fmt additional text to use as comment.
-- @param ... values to use in the formatted message.
function argerrorf(arg, fmt, ...)
  argerror(arg, fmt:format(...))
end

return M
