ordered_list = {}
list = {}
records = {}
listOpen = true

--nnids = {}
--file = io.open("nnids.txt", "r")
--for line in file:lines("l") do
--	user, nnid = string.match("(%w+),(%w+)")
--	if user ~= nil and nnid ~= nil then
--		nnids[user] = nnid
--	end
--end
--io.close(file)
file = io.open("list.txt", "r")
if file then
	for line in file:lines("l") do
		displayName = string.match(line, "(%w+)")
		if displayName ~= nil then
			user = string.lower(displayName)
			list[user] = displayName
			table.insert(ordered_list, user)
		end
	end
end
file = io.open("records.txt", "r")
if file then
	for line in file:lines("l") do
		user, wins, losses = string.match(line, "(%w+),(%d+),(%d+)")
		if user ~= nil and wins ~= nil and losses ~= nil then
			wins = tonumber(wins)
			losses = tonumber(losses)
			if wins ~= nil and losses ~= nil then
				records[user] = {}
				records[user].wins = wins
				records[user].losses = losses
			end
		end
	end
	io.close(file)
end

function finalize()
	--file = io.open("nnids.txt", "w")
	--for k,v in ipairs(nnids) do
	--	file:write(k..","..v.."\n")
	--end
	--file:close()
	file = io.open("records.txt", "w")
	cur = next(records)
	while cur ~= nil do
		file:write(cur..","..records[cur].wins..","..records[cur].losses.."\n")
		cur = next(records, cur)
	end
	file:close()
	file = io.open("list.txt", "w")
	cur = next(ordered_list)
	while cur ~= nil do
		file:write(ordered_list[cur].."\n")
		cur = next(ordered_list, cur)
	end
	file:close()
end


function rawMessageHandler(str)
	print("LUA: "..str)
end

function addPlayerToList(user, displayName)
	if not listOpen then
		asb.SendMessage("List is closed.")
		return
	end
	if list[user] ~= nil then
		asb.SendMessage("User "..displayName.." is already on the list!")
	else
		list[user] = displayName
		table.insert(ordered_list, user);
		playersAhead = #ordered_list - 1
		if playersAhead == 0 then
			asb.SendMessage("User "..displayName.." added to the list. You're up!")
		elseif playersAhead == 1 then
			asb.SendMessage("User "..displayName.." added to the list. You're next!")
		else
			asb.SendMessage("User "..displayName.." added to the list. There are ".. (#ordered_list - 1).." players ahead of you.")
		end
	end
end

function removePlayerFromList(player)
	if list[player] ~= nil then
		displayName = list[player]
		list[player] = nil
		cur = next(ordered_list)
		while cur ~= nil do
			if ordered_list[cur] == player then
				table.remove(ordered_list, cur)
			end
			cur = next(ordered_list, cur)
		end
		asb.SendMessage("Player "..displayName.." removed from the list.")
	else
		asb.SendMessage("Player "..displayName.." not found on the list.")
	end
end

commandHandlers = {}
commandHandlers["!join"] = function(user, displayName, isMod, command, arg)
	if not listOpen then
		asb.SendMessage("List is currently closed.")
		return
	end
	addPlayerToList(user, displayName)
end
commandHandlers["!joinlist"] = commandHandlers["!join"]
commandHandlers["!enter"] = commandHandlers["!join"]

commandHandlers["!list"] = function(user, displayName, isMod, command, arg)
	if not listOpen then
		asb.SendMessage("List is currently closed.")
		return
	end
	cur = next(ordered_list)
	if cur == nil then
		asb.SendMessage("List is currently empty. Use !join to join the list!")
		return
	end

	listString = ""
	i = 1

	while cur ~= nil do
		if i > 1 then
			listString = listString .. ", "
		end
		listString = listString .. i .. ". " .. list[ordered_list[cur] ]
		i = i + 1
		cur = next(ordered_list, cur)
	end

	asb.SendMessage(listString)
end

commandHandlers["!drop"] = function(user, displayName, isMod, command, arg)
	removePlayerFromList(user)
end
commandHandlers["!droplist"] = commandHandlers["!drop"]
commandHandlers["!leave"] = commandHandlers["!drop"]
commandHandlers["!resetlist"] = function(user, displayName, isMod, command, arg)
	list = {}
	ordered_list = {}
	asb.SendMessage("List reset.")
end
commandHandlers["!open"] = function(user, displayName, isMod, command, arg)
	if not isMod then return end
	listOpen = true
	asb.SendMessage("List is now open!")
end
commandHandlers["!close"] = function(user, displayName, isMod, command, arg)
	if not isMod then return end
	listOpen = false
	asb.SendMessage("List is now closed!")
end
commandHandlers["!next"] = function(user, displayName, isMod, command, arg)
	if not isMod then return end
	front = next(ordered_list)
	if front ~= nil then
		player = list[ordered_list[front] ]
		list[ordered_list[front] ] = nil

		if records[player] == nil then
			records[player] = {}
			records[player].wins = 0
			records[player].losses = 0
		end

		if command == "!win" then
			records[player].wins = records[player].wins + 1
		elseif command == "!loss" then
			records[player].losses = records[player].losses + 1
		end

		asb.SendMessage("Good games "..player.."! Use !join if you'd like to rejoin the list.")
		table.remove(ordered_list, front)

		front = next(ordered_list)
		if front ~= nil then
			asb.SendMessage("Next player up: "..list[ordered_list[front] ].."!")
		end
	end
end
commandHandlers["!win"] = commandHandlers["!next"]
commandHandlers["!loss"] = commandHandlers["!next"]
commandHandlers["!add"] = function(user, displayName, isMod, command, arg)
	if not isMod then return end
	if arg ~= "" then
		addPlayerToList(string.lower(arg), arg)
	end
end
commandHandlers["!remove"] = function(user, displayName, isMod, command, arg)
	if not isMod then return end
	if arg ~= "" then
		removePlayerFromList(string.lower(arg))
	end
end
commandHandlers["!record"] = function(user, displayName, isMod, command, arg)
	player = user
	display = displayName
	if arg ~= "" then
		player = string.lower(arg)
		display = arg
	end
	if records[player] == nil then
		asb.SendMessage("No records for player "..display.." found")
		return
	end
	-- W - L is reversed since it's for player vs. streamer
	asb.SendMessage("Record for player "..display..": "..records[player].losses.." - "..records[player].wins.." (W - L)")
end
commandHandlers["!resetrecord"] = function(user, displayName, isMod, command, arg)
	if not isMod then return end
	if arg == "" then return end
	records[string.lower(arg)] = nil
	asb.SendMessage("Record for player "..arg.." reset.")
end



function channelMessageHandler(user, displayName, message, isMod)
	command, arg = string.match(message, "(!%w+) ?(%w*)")
	if command == nil then
		return
	end
	if commandHandlers[command] ~= nil then
		commandHandlers[command](user, displayName, isMod, command, arg)
	end
end


--asb.RegisterRawMessageHandler(rawMessageHandler)
asb.RegisterChannelMessageHandler(channelMessageHandler)
asb.RegisterFinalizer(finalize)

