-- Place this file at: ~/.config/nvim/lua/plugins/l.lua
-- This will be automatically loaded by AstroNvim's plugin system

return {
	{
		"AstroNvim/astrocore",
		---@type AstroCoreOpts
		opts = {
			filetypes = {
				extension = {
					lx = "lux",
				},
			},
		},
	},
}
