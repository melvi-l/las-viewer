local function run_build(subcmd, label)
    local out = vim.fn.system('./build.sh ' .. subcmd)
    if vim.v.shell_error ~= 0 then
        vim.notify(label .. ' FAILED:\n' .. out, vim.log.levels.ERROR)
    else
        vim.notify(label .. ' done', vim.log.levels.INFO)
    end
end

vim.keymap.set('n', '<leader>g', function() run_build('all', 'all') end,
    { desc = 'Full build + run', silent = true })
vim.keymap.set('n', '<leader>l', function() run_build('lib', 'lib') end,
    { desc = 'Hot reload lib', silent = true })
vim.keymap.set('n', '<leader>s', function() run_build('shader', 'shader') end,
    { desc = 'Hot reload shader', silent = true })
