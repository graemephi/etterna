local modifierPressed = false
local forward = true

local scroller  -- just an alias for the actor that runs the commands

local function input(event)
	--SCREENMAN:SystemMessage(event.DeviceInput.button)
	if event.DeviceInput.button == "DeviceButton_right ctrl" or event.DeviceInput.button == "DeviceButton_left ctrl" then
		modifierPressed = not (event.type == "InputEventType_Release")
	end
	if event.DeviceInput.button == "DeviceButton_right shift" or event.DeviceInput.button == "DeviceButton_left shift" then
		ratePressed = not (event.type == "InputEventType_Release")
	end
	if event.DeviceInput.button == "DeviceButton_right alt" or event.DeviceInput.button == "DeviceButton_left alt" then
		bookmarkPressed = not (event.type == "InputEventType_Release")
	end
	if event.type ~= "InputEventType_Release" then
		if event.GameButton == "EffectUp" then
			if bookmarkPressed then
				scroller:queuecommand("ReplayBookmarkSet")
				return 0
			end
			forward = true
			if ratePressed then
				scroller:queuecommand("ReplayRate")
			else
				scroller:queuecommand("ReplayScroll")
			end
		elseif event.GameButton == "EffectDown" then
			if bookmarkPressed then
				scroller:queuecommand("ReplayBookmarkGoto")
				return 0
			end
			forward = false
			if ratePressed then
				scroller:queuecommand("ReplayRate")
			else
				scroller:queuecommand("ReplayScroll")
			end
		elseif event.GameButton == "Coin" then
			scroller:queuecommand("ReplayPauseToggle")
		end
	end
end

local function getNewSongPos()
	currentpos = SCREENMAN:GetTopScreen():GetSongPosition()
	newpos = currentpos + (modifierPressed and 0.1 or 5) * (forward and 1 or -1)
	--SCREENMAN:SystemMessage(string.format("%f to %f", currentpos, newpos))
	return newpos
end

local function getNewRate()
	currentrate = GAMESTATE:GetSongOptionsObject("ModsLevel_Preferred"):MusicRate()
	newrate = currentrate + (modifierPressed and 0.05 or 0.1) * (forward and 1 or -1)
	--SCREENMAN:SystemMessage(string.format("%f to %f", currentrate, newrate))
	return newrate
end

scroller =
	Def.ActorFrame {
	Name = "ScrollManager",
	OnCommand = function(self)
		SCREENMAN:GetTopScreen():AddInputCallback(input)
		scroller = self
	end,
	ReplayScrollCommand = function(self)
		newpos = getNewSongPos()
		SCREENMAN:GetTopScreen():SetReplayPosition(newpos)
	end,
	ReplayRateCommand = function(self)
		newrate = getNewRate()
		givenrate = SCREENMAN:GetTopScreen():SetReplayRate(newrate)
		if givenrate ~= nil then
			realnewrate = notShit.round(givenrate, 3)
		--SCREENMAN:SystemMessage(string.format("Set rate to %f", realnewrate))
		end
	end,
	ReplayPauseToggleCommand = function(self)
		SCREENMAN:GetTopScreen():ToggleReplayPause()
	end,
	ReplayBookmarkSetCommand = function(self)
		position = SCREENMAN:GetTopScreen():GetSongPosition()
		SCREENMAN:GetTopScreen():SetReplayBookmark(position)
	end,
	ReplayBookmarkGotoCommand = function(self)
		SCREENMAN:GetTopScreen():JumpToReplayBookmark()
	end
}
local span = 50
local x = -1 * span
local function button(txt, click)
	x = x + span
	return Widg.Button {
		text = txt,
		width = 60,
		height = 30,
		halign = 1,
		bgColor = getMainColor("highlight"),
		highlight = {color = getMainColor("positive")},
		border = {color = getMainColor("highlight"), width = 2},
		onClick = click,
		y = x + 50
	}
end

local bg =
	Widg.Rectangle {
	color = getLeaderboardColor("background"),
	width = 100,
	height = 200,
	onInit = function(self)
		local curX = getTrueX(self)
		local alignment = SCREEN_WIDTH - 45
		self:x(SCREEN_WIDTH - curX - alignment)
	end
}

local i = 1
scroller[#scroller + 1] =
	Widg.Container {
	x = SCREEN_WIDTH - 45,
	y = SCREEN_HEIGHT / 2 - 100,
	content = {
		bg,
		button(
			"Pause",
			function(self)
				SCREENMAN:GetTopScreen():ToggleReplayPause()
				local paused = GAMESTATE:IsPaused()
				self.label.actor:settext(paused and "Play" or "Pause")
			end
		),
		button(
			"Fast Forward",
			function()
				SCREENMAN:GetTopScreen():SetReplayPosition(SCREENMAN:GetTopScreen():GetSongPosition() + 5)
			end
		),
		button(
			"Rewind",
			function()
				SCREENMAN:GetTopScreen():SetReplayPosition(SCREENMAN:GetTopScreen():GetSongPosition() - 5)
			end
		)
	}
}
return scroller
