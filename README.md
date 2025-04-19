# Protocol Specification

This document describes the protocol for interacting with the key-value store server.

## Commands

### PUT
Used to store a value associated with a key.
```
Command: PUT key value\n
Response: OK\n
```
- Both key and value are strings
- The command must end with a newline character
- Server responds with "OK" followed by a newline on successful storage

### GET
Retrieves the value associated with a given key.
```
Command: GET key\n
Response: value\n
```
- The key must be a string
- The command must end with a newline character
- Server responds with the stored value followed by a newline
- If the key doesn't exist, an empty line is returned

### DELETE
Removes a key-value pair from the store.
```
Command: DELETE key\n
Response: NOT_FOUND\n
```
- The key must be a string
- The command must end with a newline character
- Server responds with "NOT_FOUND" followed by a newline if the key doesn't exist
- On successful deletion, returns "OK" followed by a newline

### CLOSE
Closes the current connection to the server.
```
Command: CLOSE\n
Response: BYE\n
```
- The command must end with a newline character
- Server responds with "BYE" followed by a newline
- After sending the response, the server closes the connection
- Any subsequent commands on this connection will fail as the connection is closed

## Note
All commands and responses are terminated with a newline character (\n).
