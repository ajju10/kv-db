# Key-Value Store with Leader-Follower Architecture

A distributed key-value store with leader-follower replication.

## Architecture

The system uses a leader-follower architecture where:
- Leader handles all write operations (PUT/DELETE)
- Followers replicate data from leader
- Both leader and followers can serve read operations (GET)
- All writes are logged before being applied to ensure durability
- Followers maintain their own log files for recovery

## Getting Started

### Prerequisites

- GCC compiler
- Make build system
- Linux environment
- Set KEYVAL_SECRET environment variable for secure leader-follower communication:
```
export KEYVAL_SECRET=your_secret_key
```

### Building

```bash
make clean
make
```

### Running Leader Server

Start the leader server (default port 5000):
```bash
./build/keyval --role leader --port 5000
```

### Running Follower Server

Start a follower server (use different ports for multiple followers):
```bash
./build/keyval --role follower --id 2 --port 5001 --leader_host 127.0.0.1 --leader_port 5000
```

Parameters:
- `--role`: Either 'leader' or 'follower'
- `--id`: Unique ID for the server (1 is reserved for leader)
- `--port`: Port to listen on
- `--leader_host`: Leader's hostname (for follower only)
- `--leader_port`: Leader's port (for follower only)

### Connecting to Server

Use any TCP client (like telnet or netcat) to connect:
```bash
nc localhost 5000
```

First send the handshake:
```
HELLO CLIENT
```

Then you can start sending commands.

## Protocol Specification

This document describes the protocol for interacting with the key-value store server.

### Commands

#### PUT
Used to store a value associated with a key.
```
Command: PUT key value\n
Response: OK\n
```
- Both key and value are strings
- The command must end with a newline character
- Server responds with "OK" followed by a newline on successful storage

#### GET
Retrieves the value associated with a given key.
```
Command: GET key\n
Response: value\n
```
- The key must be a string
- The command must end with a newline character
- Server responds with the stored value followed by a newline
- If the key doesn't exist, an empty line is returned

#### DELETE
Removes a key-value pair from the store.
```
Command: DELETE key\n
Response: NOT_FOUND\n
```
- The key must be a string
- The command must end with a newline character
- Server responds with "NOT_FOUND" followed by a newline if the key doesn't exist
- On successful deletion, returns "OK" followed by a newline

#### CLOSE
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

## Error Handling

### Client Errors
- `INVALID_COMMAND` - Malformed command or unknown command type
- `WRITE_NOT_ALLOWED` - Write operation attempted on follower
- `NOT_FOUND` - Key doesn't exist for GET/DELETE operation
- `ERROR` - Internal error during operation

### Connection Errors
- `INVALID_HANDSHAKE_MESSAGE` - Client didn't send proper handshake
- `CANNOT_ACCEPT_FOLLOWER_CONNECTION` - Follower tried to connect to non-leader
- `INVALID_SECRET_KEY` - Follower authentication failed
- `MAX_SIZE_EXCEEDED` - Leader has reached maximum follower limit

## Data Durability

- All write operations are logged before being applied
- Leader maintains `leader.log` file
- Each follower maintains its own log file (`follower_<id>.log`)
- Logs are replayed during server startup for recovery
- Log compaction happens automatically after threshold

## Limitations

- No automatic leader election
- No automatic recovery of disconnected followers
- Maximum 10 followers per leader
- ID 1 is reserved for leader
