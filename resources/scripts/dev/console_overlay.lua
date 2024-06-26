local Console = require "console"
local InputListener = require "core.input_listener"
local Input = require "bindings.input"
local Platform = require "bindings.platform"
local Overlay = require "bindings.overlay"
local nk = Overlay.nk

local isWindowMaximized = false
local baseColor = { 32, 32, 32, 255 }
local messageBkgColor = { 16, 16, 16, 255 }

local function getColorAlpha(col)
    local a = isWindowMaximized and 200 or 64
    return { col[1], col[2], col[3], a }
end

local function historyPrev()
    if Console.editTB.state[nk.EditState.NK_EDIT_ACTIVE] then
        Console:navigateHistory(-1)
        return true
    end
    return false
end

local function historyNext()
    if Console.editTB.state[nk.EditState.NK_EDIT_ACTIVE] then
        Console:navigateHistory(1)
        return true
    end
    return false
end

---@param ctx NuklearContext
local function drawFooter(ctx)
    nk.style_push(ctx, "button", "normal", getColorAlpha(baseColor))
    nk.style_push(ctx, "edit", "normal", getColorAlpha(baseColor))
    nk.style_push(ctx, "edit", "text_normal", Console:getTextColor())
    nk.style_push(ctx, "edit", "text_active", Console:getTextColor())
    nk.style_push(ctx, "edit", "text_hover", Console:getTextColor())

    nk.layout_row_begin(ctx, "dynamic", 0, 3)
    nk.layout_row_push(ctx, 0.05);
    if nk.button_label(ctx, Console.isExpanded and "<" or ">") then
        Console.isExpanded = not Console.isExpanded
    end

    nk.layout_row_push(ctx, 0.80);
    local text, state = nk.edit_string(ctx, { "commit_on_enter" }, Console:getText())

    Console:updateTextBox(text, state)

    nk.layout_row_push(ctx, 0.15);
    if nk.button_label(ctx, "Send") then
        if not Console.editTB.showPlaceholder then
            Console:send()
        end
    end

    nk.layout_row_end(ctx)

    nk.style_pop(ctx, "button", "normal")
    nk.style_pop(ctx, "edit", "normal")
    nk.style_pop(ctx, "edit", "text_normal")
    nk.style_pop(ctx, "edit", "text_active")
    nk.style_pop(ctx, "edit", "text_hover")
end

---
---@param ctx NuklearContext
local function drawConsole(ctx)
    local mainWinW, mainWinH = Platform.window.dimensions()
    Console:updateWindowSize(isWindowMaximized, mainWinW, mainWinH)
    nk.window_set_bounds(ctx, "Debug Console", Console.rect)
    nk.style_push(ctx, "window", "fixed_background", getColorAlpha(messageBkgColor))
    if nk.window_begin(ctx, "Debug Console", Console.rect, { "movable", "scalable" }) then
        isWindowMaximized = nk.window_is_hovered(ctx) or not Console.autoMinimize
        if isWindowMaximized then
            nk.layout_row_dynamic(ctx, Console.rect.h - Console.footerHeight - 28, 1)
            nk.style_push(ctx, "window", "min_row_height_padding", 1)
            if nk.group_scrolled_begin(ctx, Console.scroll, "Messages", { "scrollbar" }) then
                nk.layout_row_dynamic(ctx, 0, 1)
                for i = 1, #Console.messages do
                    local message = Console.messages[i]
                    if message.source ~= "log" or Console.logEnabled then
                        -- we must split the message every N characters because the wrap features on Nuklear does not work properly
                        local n = Console.separateEveryNCharacters
                        for j = 1, #message.text, n do
                            ---@cast n integer
                            local split = string.sub(message.text, j, j + n - 1)
                            nk.label_colored(ctx, split, message.col)
                        end
                    end
                end
                nk.group_scrolled_end(ctx)
            end
            nk.style_pop(ctx, "window", "min_row_height_padding")

            nk.layout_row_dynamic(ctx, 20, 2)
            Console.logEnabled = nk.checkbox_label(ctx, "Show Log", Console.logEnabled)
            Console.autoMinimize = nk.checkbox_label(ctx, "Auto Hide", Console.autoMinimize)
        end

        drawFooter(ctx)
    end
    nk.window_end(ctx)
    nk.style_pop(ctx, "window", "fixed_background")
end

local unregisterFromInput = function () end

local ConsoleOverlay = {}

ConsoleOverlay.init = function ()
    Console.scroll:set(0, 0)
    if #Console.messages == 0 then
        Console:addMessage("Type \"help\" on the command line to get a list of all the commands", { 255, 255, 255, 128 })
    end
    unregisterFromInput = InputListener.registerKeyPressBulk({
        { key = Input.PlatformKey.KEY_UP,   callback = historyPrev },
        { key = Input.PlatformKey.KEY_DOWN, callback = historyNext }
    })
end

---@param ctx NuklearContext
ConsoleOverlay.update = function (ctx)
    drawConsole(ctx)
end

ConsoleOverlay.close = function ()
    unregisterFromInput()
end

return ConsoleOverlay
