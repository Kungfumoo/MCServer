return
{
	HOOK_SERVER_PING =
	{
		CalledWhen = "Client pings the server from the server list.",
		DefaultFnName = "OnServerPing",  -- also used as pagename
		Desc = [[
			A plugin may implement an OnServerPing() function and register it as a Hook to process pings from
			clients in the server server list. It can change the logged in players and player capacity, as well
			as the server description and the favicon, that are displayed to the client in the server list.
		]],
		Params = {
			{ Name = "ClientHandle", Type = "{{cClientHandle}}", Notes = "The client handle that pinged the server" },
			{ Name = "ServerDescription", Type = "string", Notes = "The server description" },
			{ Name = "OnlinePlayersCount", Type = "number", Notes = "The number of players currently on the server" },
			{ Name = "MaxPlayersCount", Type = "number", Notes = "The current player cap for the server" },
			{ Name = "Favicon", Type = "string", Notes = "The base64 encoded favicon to be displayed in the server list for compatible clients" },
		},
		Returns = [[
			The plugin returns <code>res, ServerDescription, OnlinePlayersCount, MaxPlayersCount, Favicon</code>.
			res is a boolean which stops other plugins being notified of the ping if it's set to true, and the others
			are the same as the arguments, and if emitted change the values transmitted to the client. 
		]],
	},  -- HOOK_SERVER_PING
}
