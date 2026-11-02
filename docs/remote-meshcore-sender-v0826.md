# Remote MeshCore sender names — v0.8.26

The remote chat previously displayed “Peer” for incoming MeshCore messages because its sender lookup only handled direct messages. MeshCore channel messages identify the sender in their text as `Name: message`; the remote chat now extracts that name and displays only the message body. Direct messages continue to use the saved contact name. The browser already renders the sender in bold above the message. When a channel message has no valid sender prefix or a direct message has no saved contact name, the UI shows “Unknown sender.”

v0.8.23 remains the recommended stable firmware; v0.8.26 is an experimental candidate pending hardware validation.
