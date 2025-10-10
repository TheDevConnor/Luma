return {
	-- 1️⃣ Filetype mapping for .lx → lux
	{
		"AstroNvim/astrocore",
		opts = {
			filetypes = {
				extension = {
					lx = "lux",
				},
			},
		},
	},

	-- 2️⃣ LSP setup for Luma
	{
		"neovim/nvim-lspconfig",
		opts = function(_, opts)
			-- Get the path to your Luma executable
			-- Update the path to where you have it stored
			local luma_path = vim.fn.expand("~/Projects/Luma/luma")

			-- Check if the executable exists
			if vim.fn.executable(luma_path) == 0 then
				vim.notify("Luma LSP executable not found at: " .. luma_path, vim.log.levels.WARN)
				return opts
			end

			local lspconfig = require("lspconfig")
			local configs = require("lspconfig.configs")

			-- Register Luma LSP if not already registered
			if not configs.luma then
				configs.luma = {
					default_config = {
						cmd = { luma_path, "-lsp" },
						filetypes = { "lux" },
						root_dir = function(fname)
							-- Look for git root, or use current directory
							return lspconfig.util.find_git_ancestor(fname) or lspconfig.util.path.dirname(fname)
						end,
						single_file_support = true,
						-- Add some logging to help debug
						on_attach = function(client, bufnr)
							vim.notify("Luma LSP attached to buffer " .. bufnr, vim.log.levels.INFO)
						end,
						-- Capture stderr for debugging
						on_exit = function(code, signal, client_id)
							if code ~= 0 then
								vim.notify(
									string.format("Luma LSP exited with code %d, signal %d", code, signal),
									vim.log.levels.ERROR
								)
							end
						end,
					},
				}
			end

			-- Get capabilities - works with both old and new AstroNvim versions
			local capabilities = vim.lsp.protocol.make_client_capabilities()

			-- Try to enhance with cmp-nvim-lsp if available
			local has_cmp, cmp_nvim_lsp = pcall(require, "cmp_nvim_lsp")
			if has_cmp then
				capabilities = cmp_nvim_lsp.default_capabilities(capabilities)
			end

			-- Setup the server
			lspconfig.luma.setup({
				capabilities = capabilities,
			})

			return opts
		end,
	},
}
