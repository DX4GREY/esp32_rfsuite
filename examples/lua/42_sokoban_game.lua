-- Sokoban for the 152x86 Lua GUI canvas.
-- UP=up, DOWN=down, A=right, B=left.
-- Grid: 15 cols, 8 rows. Cell size: 10px. Offset to center slightly.

local cols, rows, cell = 15, 8, 10
local offsetX, offsetY = 1, 3

-- Game states
local STATE_PLAY = 1
local STATE_WIN = 2
local current_state = STATE_PLAY
local MAX_GAME_MS = 180000 -- End cleanly after three minutes.

-- Level definition (0: empty, 1: wall, 2: target, 3: box, 4: box on target, 5: player)
-- A simple small level for demonstration
local initial_map = {
    {0,0,1,1,1,1,1,0,0,0,0,0,0,0,0},
    {0,0,1,2,5,0,1,0,0,0,0,0,0,0,0},
    {0,0,1,0,3,0,1,0,1,1,1,0,0,0,0},
    {0,1,1,1,0,0,1,0,1,2,1,0,0,0,0},
    {0,1,2,1,0,3,1,1,1,0,1,0,0,0,0},
    {0,1,0,1,0,0,0,0,0,0,1,0,0,0,0},
    {0,1,3,0,0,1,1,1,1,1,1,0,0,0,0},
    {0,1,1,1,1,1,0,0,0,0,0,0,0,0,0}
}

local map = {}
local player = {x = 0, y = 0}

-- Copy initial map to working map and find player
local function load_level()
    map = {}
    for y = 1, rows do
        map[y] = {}
        for x = 1, cols do
            map[y][x] = initial_map[y][x]
            if map[y][x] == 5 then
                player.x = x
                player.y = y
                map[y][x] = 0 -- Player is stored separately, remove from static map
            end
        end
    end
    current_state = STATE_PLAY
end

local function draw_cell(x, y, color, solid)
    rf.gui_rect(offsetX + (x-1) * cell, offsetY + (y-1) * cell, cell - 1, cell - 1, color, solid)
end

local function check_win()
    for y = 1, rows do
        for x = 1, cols do
            if map[y][x] == 3 then -- Jika masih ada kotak yang belum di target
                return false
            end
        end
    end
    return true
end

local function move_player(dx, dy)
    local target_x = player.x + dx
    local target_y = player.y + dy
    
    -- Check boundaries (meskipun dinding biasanya melindungi)
    if target_x < 1 or target_x > cols or target_y < 1 or target_y > rows then return false end

    local target_cell = map[target_y][target_x]

    -- Hit a wall
    if target_cell == 1 then return false end

    -- Empty or Target
    if target_cell == 0 or target_cell == 2 then
        player.x = target_x
        player.y = target_y
        return true
    end

    -- Hit a box (3) or box on target (4)
    if target_cell == 3 or target_cell == 4 then
        local beyond_x = target_x + dx
        local beyond_y = target_y + dy
        
        if beyond_x < 1 or beyond_x > cols or beyond_y < 1 or beyond_y > rows then return false end
        
        local beyond_cell = map[beyond_y][beyond_x]

        -- Can we push the box? (Beyond cell must be empty or target)
        if beyond_cell == 0 or beyond_cell == 2 then
            -- Move the box
            if target_cell == 3 then map[target_y][target_x] = 0
            else map[target_y][target_x] = 2 end -- was on target, leave target behind

            if beyond_cell == 0 then map[beyond_y][beyond_x] = 3
            else map[beyond_y][beyond_x] = 4 end -- pushed onto target

            -- Move player
            player.x = target_x
            player.y = target_y
            
            if check_win() then
                current_state = STATE_WIN
            end
            return true
        end
    end

    return false
end

local function draw_level()
    rf.gui_clear()

    for y = 1, rows do
        for x = 1, cols do
            local val = map[y][x]
            if val == 1 then draw_cell(x, y, "green", false)
            elseif val == 2 then draw_cell(x, y, "yellow", false)
            elseif val == 3 then draw_cell(x, y, "accent", true)
            elseif val == 4 then draw_cell(x, y, "yellow", true)
            end
        end
    end

    draw_cell(player.x, player.y, "red", true)
end

local function draw_win()
    rf.gui_clear()
    rf.gui_text(40, 30, "LEVEL CLEAR!", "yellow")
    rf.gui_text(35, 50, "A to Restart", "accent")
end

-- Initialize game
load_level()

rf.gui_begin("SOKOBAN")
rf.gui_footer("B LEFT", "U/D", "A RIGHT")

-- Variabel untuk mencegah input berulang terlalu cepat (simple debounce)
local wait_release = false
local needs_redraw = true
local running = true
local started_at = rf.millis()

while running and rf.millis() - started_at < MAX_GAME_MS do
    local up = rf.button("up")
    local down = rf.button("down")
    local left = rf.button("b")
    local right = rf.button("a")
    local pressed = up or down or left or right

    -- A+B is reserved as an explicit escape gesture. A finite session also
    -- guarantees that this interactive script cannot consume the VM budget
    -- forever when the device is left unattended.
    if left and right then
        rf.gui_close()
        running = false
    elseif current_state == STATE_PLAY then
        -- Handle Input (Penting: Di Sokoban kita butuh penekanan tombol satu per satu, bukan menahan)
        local dx, dy = 0, 0

        if up then dy = -1
        elseif down then dy = 1
        elseif left then dx = -1
        elseif right then dx = 1
        end

        if pressed and not wait_release then
            if move_player(dx, dy) then needs_redraw = true end
            wait_release = true
        elseif not pressed then
            wait_release = false
        end
    elseif current_state == STATE_WIN then
        if right and not wait_release then
            load_level()
            wait_release = true
            needs_redraw = true
        elseif not pressed then
            wait_release = false
        end
    end

    if needs_redraw then
        if current_state == STATE_WIN then draw_win()
        else draw_level() end
        needs_redraw = false
    end

    rf.delay(120)
end

if running then
    rf.gui_clear()
    rf.gui_text(35, 34, "SESSION ENDED", "yellow")
    rf.gui_text(25, 50, "B to return", "accent")
end
