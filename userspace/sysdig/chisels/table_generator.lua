--[[
Copyright (C) 2013-2014 Draios inc.
 
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
--]]

-- Chisel description
description = "Given two filter fields, a key and a value, this chisel creates and renders to the screen a table."
short_description = "FD bytes group by"
category = "IO"
hidden = true

-- Chisel argument list
args = 
{
	{
		name = "key", 
		description = "the filter field used for grouping", 
		argtype = "string"
	},
	{
		name = "keydesc", 
		description = "human readable description for the key", 
		argtype = "string"
	},
	{
		name = "value", 
		description = "the value to count for every key", 
		argtype = "string"
	},
	{
		name = "valuedesc", 
		description = "human readable description for the value", 
		argtype = "string"
	},
	{
		name = "filter", 
		description = "the filter to apply", 
		argtype = "string"
	},
	{
		name = "top_number", 
		description = "maximum number of elements to display", 
		argtype = "string"
	},
	{
		name = "value_units", 
		description = "how to render the values in the result. Can be 'bytes', 'time', 'timepct', or 'none'.", 
		argtype = "string"
	},
}

require "common"
terminal = require "ansiterminal"

grtable = {}
filter = ""
islive = false

vizinfo = 
{
	key_fld = "",
	key_desc = "",
	value_fld = "",
	value_desc = "",
	value_units = "none",
	top_number = 0,
	output_format = "normal"
}

-- Argument notification callback
function on_set_arg(name, val)
	if name == "key" then
		vizinfo.key_fld = val
		return true
	elseif name == "keydesc" then
		vizinfo.key_desc = val
		return true
	elseif name == "value" then
		vizinfo.value_fld = val
		return true
	elseif name == "valuedesc" then
		vizinfo.value_desc = val
		return true
	elseif name == "filter" then
		filter = val
		return true
	elseif name == "top_number" then
		vizinfo.top_number = tonumber(val)
		return true
	elseif name == "value_units" then
		vizinfo.value_units = val
		return true
	end

	return false
end

function on_init()
	-- Request the fields we need
	fkey = chisel.request_field(vizinfo.key_fld)
	fvalue = chisel.request_field(vizinfo.value_fld)

	-- set the filter
	if filter ~= "" then
		chisel.set_filter(filter)
	end
	
	return true
end

function on_capture_start()
	islive = sysdig.is_live()
	vizinfo.output_format = sysdig.get_output_format()

	if islive then
		chisel.set_interval_s(1)
		if vizinfo.output_format ~= "json" then
			terminal.clearscreen()
			terminal.hidecursor()
		end
	end
	
	return true
end

function on_event()
	key = evt.field(fkey)
	value = evt.field(fvalue)

	if key ~= nil and value ~= nil and value > 0 then
		entryval = grtable[key]

		if entryval == nil then
			grtable[key] = value
		else
			grtable[key] = grtable[key] + value
		end
	end

	return true
end

function on_interval(ts_s, ts_ns, delta)	
	if vizinfo.output_format ~= "json" then
		terminal.clearscreen()
		terminal.goto(0, 0)
	end
	
	print_sorted_table(grtable, ts_s, 0, delta, vizinfo)

	-- Clear the table
	grtable = {}
	
	return true
end

function on_capture_end(ts_s, ts_ns, delta)
	if islive and vizinfo.output_format ~= "json" then
		terminal.clearscreen()
		terminal.goto(0 ,0)
		terminal.showcursor()
		return true
	end
	
	print_sorted_table(grtable, ts_s, 0, delta, vizinfo)
	
	return true
end
