local t =
	Widg.Container {
	y = SCREEN_HEIGHT / 10
}
local leaderboardEnabled = playerConfig:get_data(pn_to_profile_slot(PLAYER_1)).leaderboardEnabled and DLMAN:IsLoggedIn()
if not leaderboardEnabled then
	return t
end
local CRITERIA = "GetWifeScore"
local NUM_ENTRIES = 5
local ENTRY_HEIGHT = IsUsingWideScreen() and 35 or 20
local WIDTH = SCREEN_WIDTH * (IsUsingWideScreen() and 0.3 or 0.275)
local jdgs = {
	-- Table of judgments for the judgecounter
	"TapNoteScore_W1",
	"TapNoteScore_W2",
	"TapNoteScore_W3",
	"TapNoteScore_W4",
	"TapNoteScore_W5"
}

if not DLMAN:GetCurrentRateFilter() then
	DLMAN:ToggleRateFilter()
end
local onlineScores = DLMAN:RequestChartLeaderBoard(GAMESTATE:GetCurrentSteps(PLAYER_1):GetChartKey())
local sortFunction = function(h1, h2)
	return h1[CRITERIA](h1) > h2[CRITERIA](h2)
end
table.sort(onlineScores, sortFunction)
local curScore
curScore = {
	GetDisplayName = function()
		return DLMAN:GetUsername()
	end,
	GetWifeGrade = function()
		return curScore.curGrade
	end,
	GetWifeScore = function()
		return curScore.curWifeScore
	end,
	GetSkillsetSSR = function()
		return -1
	end,
	GetJudgmentString = function()
		local str = ""
		for i, v in ipairs(jdgs) do
			str = str .. curScore.jdgVals[v] .. " | "
		end
		return str .. "x" .. curScore.combo
	end
}
curScore.combo = 0
curScore.curWifeScore = 0
curScore.curGrade = "Grade_Tier02"
curScore.jdgVals = {
	["TapNoteScore_W1"] = 0,
	["TapNoteScore_W2"] = 0,
	["TapNoteScore_W3"] = 0,
	["TapNoteScore_W4"] = 0,
	["TapNoteScore_W5"] = 0,
	["TapNoteScore_Miss"] = 0
}
local scoreboard = {}
for i = 1, NUM_ENTRIES - 1 do
	scoreboard[i] = onlineScores[i]
end
local done = false
for i = 1, NUM_ENTRIES do
	if not done and not scoreboard[i] then
		scoreboard[i] = curScore
		done = true
	end
end

local entryActors = {}
for i = 1, NUM_ENTRIES do
	entryActors[i] = {}
end
function scoreEntry(i)
	local entryActor
	local entry =
		Widg.Container {
		x = WIDTH / 40,
		y = (i - 1) * ENTRY_HEIGHT * 1.3,
		onInit = function(self)
			entryActor = self
			entryActors[i]["container"] = self
			self.update = function(self, hs)
				self:visible(not (not hs))
			end
			self:update(scoreboard[i])
		end
	}
	entry:add(
		Widg.Rectangle {
			width = WIDTH,
			height = ENTRY_HEIGHT,
			color = getLeaderboardColor("background"),
			halign = 0.5
		}
	)
	local labelContainer =
		Widg.Container {
		x = WIDTH / 5
	}
	entry:add(labelContainer)
	local y
	local addLabel = function(name, fn, x, y)
		y = (y or 0) - (IsUsingWideScreen() and 0 or ENTRY_HEIGHT / 3.2)
		labelContainer:add(
			Widg.Label {
				onInit = function(self)
					entryActors[i][name] = self
					self.update = function(self, hs)
						if hs then
							self:visible(true)
							fn(self, hs)
						else
							self:visible(false)
						end
					end
					self:update(scoreboard[i])
				end,
				halign = 0,
				scale = 0.4,
				x = (x - WIDTH / 2) * 0.4,
				y = 10 + y,
				color = getLeaderboardColor("text")
			}
		)
	end
	addLabel(
		"rank",
		function(self, hs)
			self:settext(tostring(i))
		end,
		5,
		ENTRY_HEIGHT / 4
	)
	addLabel(
		"ssr",
		function(self, hs)
			local ssr = hs:GetSkillsetSSR("Overall")
			if ssr < 0 then
				self:settext("")
			else
				self:settextf("%.2f", ssr):diffuse(byMSD(ssr))
			end
		end,
		WIDTH / 5
	)
	addLabel(
		"name",
		function(self, hs)
			self:settext(hs:GetDisplayName())
		end,
		WIDTH / 1.3
	)
	--WIDTH - 84
	addLabel(
		"wife",
		function(self, hs)
			self:settextf("%05.2f%%", hs:GetWifeScore() * 100):diffuse(byGrade(hs:GetWifeGrade()))
		end,
		1.8 * WIDTH
	)
	addLabel(
		"judges",
		function(self, hs)
			self:settext(hs:GetJudgmentString())
		end,
		WIDTH / 5,
		ENTRY_HEIGHT / 2
	)
	return entry
end
for i = 1, NUM_ENTRIES do
	t[#t + 1] = scoreEntry(i)
end

t.ComboChangedMessageCommand = function(self, params)
	curScore.combo = params.PlayerStageStats and params.PlayerStageStats:GetCurrentCombo() or params.OldCombo
end
t.JudgmentMessageCommand = function(self, params)
	if curScore.jdgVals[params.Judgment] then
		curScore.jdgVals[params.Judgment] = params.Val
	end
	-- params.curWifeScore retrieves the Judgment Message curWifeScore which is a raw number for calculations; very large
	-- the online highscore curWifeScore is the wife percent...
	-- params.WifePercent is our current calculated wife percent.
	local old = curScore.curWifeScore
	curScore.curWifeScore = notShit.floor(params.WifePercent * 100) / 10000
	if old ~= curScore.curWifeScore then
		table.sort(scoreboard, sortFunction)
		for i, entry in ipairs(entryActors) do
			for name, label in pairs(entry) do
				label:update(scoreboard[i])
			end
		end
	end
end

return t
