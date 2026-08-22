-- Sokoban: 10 fixed levels, optimized for the Lua instruction limit.
-- UP/DOWN move vertically, B/A move left/right, A+B returns to the list.

local COLS, ROWS, CELL = 15, 8, 10
local OX, OY = 1, 3
local EMPTY, WALL, TARGET, BOX, BOX_TARGET = 0, 1, 2, 3, 4
local PLAY, WIN, COMPLETE = 1, 2, 3
local MAX_TICKS = 1200

-- Every row is exactly 15 characters. Legend: # wall, . target,
-- $ box, * box on target, @ player.
local levels = {
    {
        "  #######      ",
        "  #  .  #      ",
        "  #  $  #      ",
        "  #  @  #      ",
        "  #     #      ",
        "  #######      ",
        "               ",
        "               "
    },
    {
        " #########     ",
        " # .   . #     ",
        " # $   $ #     ",
        " #   @   #     ",
        " #       #     ",
        " #########     ",
        "               ",
        "               "
    },
    {
        " #########     ",
        " # . . . #     ",
        " # $ $ $ #     ",
        " #       #     ",
        " #   @   #     ",
        " #########     ",
        "               ",
        "               "
    },
    {
        "  #########    ",
        "  # . .   #    ",
        "  # $ $   #    ",
        "  #   #   #    ",
        "  #   @   #    ",
        "  #########    ",
        "               ",
        "               "
    },
    {
        " ##########    ",
        " #  . .   #    ",
        " #  $ $   #    ",
        " #        #    ",
        " # #  @   #    ",
        " ##########    ",
        "               ",
        "               "
    },
    {
        " ###########   ",
        " # . . .   #   ",
        " # $ $ $   #   ",
        " #     #   #   ",
        " #   @     #   ",
        " ###########   ",
        "               ",
        "               "
    },
    {
        "  #########    ",
        "  # . . . #    ",
        "  # $ $ $ #    ",
        "  #   #   #    ",
        "  #   @   #    ",
        "  #       #    ",
        "  #########    ",
        "               "
    },
    {
        "  #########    ",
        "  # .   . #    ",
        "  # $ # $ #    ",
        "  #   #   #    ",
        "  #   @   #    ",
        "  #########    ",
        "               ",
        "               "
    },
    {
        " ###########   ",
        " # . . . . #   ",
        " # $ $ $ $ #   ",
        " #         #   ",
        " #  ##     #   ",
        " #    @    #   ",
        " ###########   ",
        "               "
    },
    {
        "###############",
        "# . . . . .   #",
        "# $ $ $ $ $   #",
        "#             #",
        "#   ###       #",
        "#      @      #",
        "#             #",
        "###############"
    }
}

local map = {}
local player = {x=1, y=1}
local level_index, boxes_remaining, state = 1, 0, PLAY

local function inside(x, y)
    return x >= 1 and x <= COLS and y >= 1 and y <= ROWS
end

local function rect_cell(x, y, color, solid)
    rf.gui_rect(OX+(x-1)*CELL, OY+(y-1)*CELL, CELL-1, CELL-1, color, solid)
end

-- Redraw only one cell. Movement therefore costs a constant amount of work.
local function draw_tile(x, y, show_player)
    rect_cell(x, y, "black", true)
    local v = map[y][x]
    if v == WALL then
        rect_cell(x, y, "green", false)
    elseif v == TARGET then
        rect_cell(x, y, "yellow", false)
    elseif v == BOX then
        rect_cell(x, y, "accent", true)
    elseif v == BOX_TARGET then
        rect_cell(x, y, "yellow", true)
    end
    if show_player then rect_cell(x, y, "red", true) end
end

local function draw_board()
    rf.gui_clear()
    for y=1,ROWS do
        for x=1,COLS do
            if map[y][x] ~= EMPTY then draw_tile(x, y, false) end
        end
    end
    draw_tile(player.x, player.y, true)
end

local function load_level(index)
    level_index, boxes_remaining, state = index, 0, PLAY
    for y=1,ROWS do
        map[y] = {}
        local row = levels[index][y]
        for x=1,COLS do
            local c = string.sub(row, x, x)
            local v = EMPTY
            if c == "#" then v = WALL
            elseif c == "." then v = TARGET
            elseif c == "$" then v = BOX; boxes_remaining = boxes_remaining + 1
            elseif c == "*" then v = BOX_TARGET
            elseif c == "@" then player.x, player.y = x, y end
            map[y][x] = v
        end
    end
    rf.gui_footer("B LEFT", "L"..index.."/10", "A RIGHT")
    draw_board()
end

local function move(dx, dy)
    local ox, oy = player.x, player.y
    local tx, ty = ox+dx, oy+dy
    if not inside(tx, ty) or map[ty][tx] == WALL then return end
    local v = map[ty][tx]

    if v == EMPTY or v == TARGET then
        player.x, player.y = tx, ty
        draw_tile(ox, oy, false)
        draw_tile(tx, ty, true)
        return
    end

    if v ~= BOX and v ~= BOX_TARGET then return end
    local bx, by = tx+dx, ty+dy
    if not inside(bx, by) then return end
    local beyond = map[by][bx]
    if beyond ~= EMPTY and beyond ~= TARGET then return end

    -- Update the remaining-box counter instead of scanning all 120 cells.
    if v == BOX_TARGET then boxes_remaining = boxes_remaining + 1 end
    if beyond == TARGET then boxes_remaining = boxes_remaining - 1 end
    map[ty][tx] = (v == BOX_TARGET) and TARGET or EMPTY
    map[by][bx] = (beyond == TARGET) and BOX_TARGET or BOX
    player.x, player.y = tx, ty
    draw_tile(ox, oy, false)
    draw_tile(tx, ty, true)
    draw_tile(bx, by, false)

    if boxes_remaining == 0 then
        state = WIN
        rf.gui_footer("A+B EXIT", "CLEAR!", "A NEXT")
    end
end

local function show_complete()
    state = COMPLETE
    rf.gui_clear()
    rf.gui_text(32, 20, "ALL LEVELS", "yellow")
    rf.gui_text(43, 36, "CLEAR!", "green")
    rf.gui_text(24, 56, "A = PLAY AGAIN", "accent")
    rf.gui_footer("A+B EXIT", "10/10", "A REPLAY")
end

rf.gui_begin("SOKOBAN")
load_level(1)

local running, held, ticks = true, false, 0
while running and ticks < MAX_TICKS do
    local up = rf.button("up")
    local down = rf.button("down")
    local left = rf.button("b")
    local right = rf.button("a")
    local pressed = up or down or left or right

    if left and right then
        rf.gui_close()
        running = false
    elseif pressed and not held then
        held = true
        if state == PLAY then
            if up then move(0,-1)
            elseif down then move(0,1)
            elseif left then move(-1,0)
            elseif right then move(1,0) end
        elseif right and state == WIN then
            if level_index < #levels then load_level(level_index+1)
            else show_complete() end
        elseif right and state == COMPLETE then
            load_level(1)
        end
    elseif not pressed then
        held = false
    end

    ticks = ticks + 1
    rf.delay(150)
end

-- A finite polling budget prevents this example from exhausting the VM quota.
if running then rf.gui_close() end
