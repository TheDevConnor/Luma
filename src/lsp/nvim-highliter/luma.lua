return {
  -- Filetype mapping for .lx → luma
  {
    "AstroNvim/astrocore",
    opts = {
      filetypes = {
        extension = {
          lx = "luma",
        },
      },
    },
  },

  -- LSP setup for Luma
  {
    "neovim/nvim-lspconfig",
    opts = function(_, opts)
      local luma_path = vim.fn.expand("~/Projects/Luma/luma")
      if vim.fn.executable(luma_path) == 0 then
        vim.notify("Luma LSP not found at " .. luma_path, vim.log.levels.WARN)
        return opts
      end

      local lspconfig = require("lspconfig")
      local configs = require("lspconfig.configs")

      if not configs.luma then
        configs.luma = {
          default_config = {
            cmd = { luma_path, "-lsp" },
            filetypes = { "luma" },
            root_dir = function(fname)
              return lspconfig.util.find_git_ancestor(fname)
                or lspconfig.util.path.dirname(fname)
            end,
            single_file_support = true,
            on_attach = function(_, bufnr)
              vim.notify("Luma LSP attached to buffer " .. bufnr, vim.log.levels.INFO)
            end,
            on_exit = function(code, signal, _)
              if code ~= 0 then
                vim.notify(
                  string.format("Luma LSP exited with code %d (signal %d)", code, signal),
                  vim.log.levels.ERROR
                )
              end
            end,
          },
        }
      end

      local capabilities = vim.lsp.protocol.make_client_capabilities()
      local has_cmp, cmp_nvim_lsp = pcall(require, "cmp_nvim_lsp")
      if has_cmp then
        capabilities = cmp_nvim_lsp.default_capabilities(capabilities)
      end

      lspconfig.luma.setup({
        capabilities = capabilities,
      })

      return opts
    end,
  },

  -- Syntax highlighting (load custom Vim syntax file)
  {
    "nvim-treesitter/nvim-treesitter",
    init = function()
      vim.api.nvim_create_autocmd("FileType", {
        pattern = "luma",
        callback = function()
          local syntax_file = vim.fn.expand("~/.config/nvim/syntax/luma.vim")
          if vim.fn.filereadable(syntax_file) == 1 then
            vim.cmd("runtime syntax/luma.vim")
          end
        end,
      })
    end,
  },
}

