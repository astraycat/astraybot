
function rawMessageHandler(str)
	print("LUA: "..str)
end

function channelMessageHandler(user, displayName, message, isMod)
	print(displayName.."["..user.."]["..isMod.."]: "..message)
end

asb.RegisterRawMessageHandler(rawMessageHandler)
asb.RegisterChannelMessageHandler(channelMessageHandler)

