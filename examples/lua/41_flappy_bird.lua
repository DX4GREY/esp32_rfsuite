-- Flappy Box for 152x86 Lua GUI canvas.
-- A=Jump.
local bird_x, bird_y = 25, 43
local bird_size = 6
local vy = 0
local gravity = 1
local jump_strength = -6
local pipe_x = 152
local pipe_w = 12
local pipe_gap = 35
local pipe_hole_y = 20
local score = 0
local alive = true

math.randomseed(rf.millis())

local function reset_pipe()
    pipe_x = 152
    pipe_hole_y = math.random(10, 86 - pipe_gap - 10)
end

reset_pipe()

rf.gui_begin("FLAPPY BOX")
rf.gui_footer("", "", "A JUMP")

while alive do
    -- Input
    if rf.button("a") then
        vy = jump_strength
    end

    -- Physics (Simplified)
    vy = vy + gravity
    -- Limit fall speed
    if vy > 6 then vy = 6 end 
    bird_y = bird_y + vy

    -- Pipe movement
    pipe_x = pipe_x - 3
    if pipe_x < -pipe_w then
        score = score + 1
        reset_pipe()
    end

    -- Floor & Ceiling Collision
    if bird_y < 0 or bird_y + bird_size > 86 then
        alive = false
    end
    
    -- Pipe Collision
    if bird_x + bird_size > pipe_x and bird_x < pipe_x + pipe_w then
        if bird_y < pipe_hole_y or bird_y + bird_size > pipe_hole_y + pipe_gap then
            alive = false
        end
    end

    -- Render
    rf.gui_clear()
    
    -- Bird
    rf.gui_rect(bird_x, math.floor(bird_y), bird_size, bird_size, "accent", true)
    
    -- Pipe Top
    rf.gui_rect(pipe_x, 0, pipe_w, pipe_hole_y, "green", true)
    
    -- Pipe Bottom
    local bottom_y = pipe_hole_y + pipe_gap
    rf.gui_rect(pipe_x, bottom_y, pipe_w, 86 - bottom_y, "green", true)

    rf.gui_text(2, 10, "S:" .. tostring(score), "yellow")
    
    -- ~25 FPS 
    rf.delay(40)
end

rf.gui_clear()
rf.gui_text(47, 25, "GAME OVER", "red")
rf.gui_text(50, 39, "SCORE " .. tostring(score), "yellow")
rf.gui_text(33, 56, "A=REPLAY B=LIST", "accent")
rf.gui_footer("B LIST", "", "A REPLAY")
print("Flappy score", score)