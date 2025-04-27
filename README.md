# Key-Value Store with Leader-Follower Architecture

A distributed key-value store with leader-follower replication.

## Architecture

The system uses a leader-follower architecture where:
- Leader handles all write operations (PUT/DELETE)
- Followers replicate data from leader in real-time
- Both leader and followers can serve read operations (GET)
- All writes are logged before being applied to ensure durability
- Followers maintain their own log files for recovery
- Secure follower authentication using shared secret key
- Atomic log syncing during follower initialization

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

### Handshakes

#### Client Handshake
```
Client: HELLO CLIENT\n
Server: HELLO SERVER\n
```
After successful handshake, client can start sending commands.

#### Follower Handshake
```
Follower: HELLO FOLLOWER|<id>:<secret_key>\n
Leader: HELLO LEADER\n
```
After successful handshake, leader initiates the sync process.

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

## Follower Syncing

The system implements a robust follower syncing mechanism:

### Initial Sync
1. When a follower starts up, it connects to the leader using secure authentication
2. Authentication uses a shared secret key (KEYVAL_SECRET environment variable)
3. After successful authentication, the follower initiates a sync process
4. Leader sends all existing data to the follower in an atomic operation
5. Follower creates a temporary log file during sync
6. On successful sync, the temporary file atomically replaces the old log
7. Follower replays the log to rebuild its state

### Real-time Replication
1. After initial sync, follower maintains an open connection with leader
2. Leader broadcasts all write operations (PUT/DELETE) to connected followers
3. Followers apply operations in order received from leader
4. Each operation is logged before being applied to maintain durability
5. Failed operations are logged to help with troubleshooting

### Data Safety
- Temporary files used during sync to prevent corruption
- Atomic renames ensure log consistency
- Log compaction happens automatically to prevent unbounded growth
- Each follower maintains independent logs for recovery

## Limitations

- No automatic leader election
- No automatic recovery of disconnected followers
- Maximum 10 followers per leader
- ID 1 is reserved for leader
