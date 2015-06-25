ordered_list = {}
list = {}
listOpen = true

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
		asb.SendMessage("User "..displayName.." added to the list. There are ".. (#ordered_list - 1).." players ahead of you.")
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

function channelMessageHandler(user, displayName, message, isMod)
	if message == "!joinlist" or message == "!join" or message == "!enter" then
		addPlayerToList(user, displayName)
	elseif message == "!list" then
		if not listOpen then
			asb.SendMessage("List is currently closed.")
			return
		end
		cur = next(ordered_list)
		if cur == nil then
			asb.SendMessage("List is currently empty.")
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
	elseif message == "!drop" or message == "!droplist" or message == "!leave" then
		removePlayerFromList(user)
	elseif isMod and message == "!open" then
		listOpen = true
		asb.SendMessage("List is now open!")
	elseif isMod and message == "!close" then
		listOpen = false
		asb.SendMessage("List is now closed!")
	elseif isMod and (message == "!win" or message == "!loss" or message == "!next") then
		front = next(ordered_list)
		if front ~= nil then
			player = list[ordered_list[front] ]
			list[ordered_list[front] ] = nil

			asb.SendMessage("Good games "..player.."! Use !join if you'd like to rejoin the list.")
			table.remove(ordered_list, front)

			front = next(ordered_list)
			if front ~= nil then
				asb.SendMessage("Next player up: "..list[ordered_list[front] ].."!")
			end
		end
	end
	if isMod then
		match = string.match(message, "!add (%w+)")
		if match ~= nil then
			addPlayerToList(string.lower(match), match)
			return
		end
		match = string.match(message, "!remove (%w+)")
		if match ~= nil then
			player = string.lower(match)
			removePlayerFromList(player)
		end 
	end
end

function finalize()
end

--asb.RegisterRawMessageHandler(rawMessageHandler)
asb.RegisterChannelMessageHandler(channelMessageHandler)
asb.RegisterFinalizer(finalize)

