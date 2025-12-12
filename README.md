# TCP Client–Server Card Game with AI (C)

This project is a **networked multiplayer card game** implemented in **pure C**, using **TCP sockets**.
It was developed as part of a **Systems & Networks (Systèmes et Réseaux)** academic project.

The game follows a **client–server architecture**, supports **multiple players**, and includes:

* Human players connecting from different machines
* A basic AI player
* An optional “smart” AI player that can use an external LLM API (Grok / xAI)

All game logic and state are managed **server-side**, while clients act as simple terminals sending commands.

---

## Features

* TCP server handling multiple concurrent clients
* Turn-based card game logic (similar to *6 qui prend*)
* Real-time game state displayed on the server
* Text-based protocol (human-readable)
* Human players + AI players
* Deterministic “dumb” AI
* Optional LLM-powered AI (via HTTP API)
* Portable POSIX C (Linux / macOS)
* No OOP, no frameworks, no external game libraries

---

## Architecture Overview

```
┌─────────┐     TCP      ┌─────────┐
│ Client  │ ───────────▶ │         │
│ (Human) │              │         │
└─────────┘              │         │
                          │ Server  │
┌─────────┐     TCP      │ (Game   │
│ Client  │ ───────────▶ │  Logic) │
│ (AI)    │              │         │
└─────────┘              │         │
                          └─────────┘
```

* **Server**

  * Owns the game state
  * Manages turns
  * Resolves card placement
  * Displays the full game state
* **Clients**

  * Send commands
  * Display personal hand + table state
  * Can run on different machines

---

## Game Rules (Summary)

* Each player has a hand of cards.
* There are **4 rows** on the table.
* On each turn:

  1. All players choose a card.
  2. Cards are revealed in ascending order.
  3. Each card is placed on the closest valid row.
  4. If no valid row exists, the player must take a row.
* The goal is to **avoid taking rows**.

The exact scoring/penalty logic is implemented server-side.

---

## Project Structure

```
.
├── server.c          # Main TCP server
├── client.c          # Human player client
├── robot.c           # Basic AI client
├── smart_robot.c     # LLM-based AI (optional)
├── game.c            # Game mechanics
├── game.h
├── net.c             # Socket utilities
├── net.h
├── util.c            # Helpers (parsing, I/O)
├── util.h
├── common.h          # Shared constants/types
├── Makefile
└── README.md
```

---

## Build Instructions

### Requirements

* GCC or Clang
* POSIX system (Linux / macOS)
* `make`

### Compile

```bash
make
```

This produces:

* `server`
* `client`
* `robot` (AI)

---

## Running the Game

### 1. Start the server

```bash
./server <port> <number_of_players>
```

Example:

```bash
./server 5050 2
```

### 2. Start clients (on same or different machines)

```bash
./client <server_ip> <port> <player_name>
```

Example:

```bash
./client 127.0.0.1 5050 adil
```

### 3. Start AI client

```bash
./robot <server_ip> <port> ai
```

---

## Gameplay (Client Side)

Example output:

```
R1: 12 | R2: 7 | R3: 30 | R4: 23
MAIN 2 8 29 32 48 49 85 87 94 100
DEMANDE_CARTE
>
```

To play, type **one card value** and press Enter:

```
JOUER 29
```

---

## AI Players

### Basic AI

* Chooses the lowest-risk card
* No randomness
* No external dependencies

### Smart AI (Optional)

* Sends the game state to an LLM API
* Expects a numeric card choice as response
* Uses HTTP requests
* API key is **never committed** to Git

API keys should be stored in:

```
.env
config.local
```

These files must be added to `.gitignore`.

---

## Network Protocol (Simplified)

Server → Client:

```
TABLE R1: ... | R2: ... | R3: ... | R4: ...
MAIN <cards>
DEMANDE_CARTE
```

Client → Server:

```
<card_number>
```

All messages are line-based (`\n`).

---

## Error Handling

* Invalid card → rejected
* Disconnected client → handled server-side
* Partial reads → buffered
* Blocking sockets (simple, predictable behavior)

---

## Limitations

* No GUI (terminal only)
* Blocking I/O
* No encryption (plain TCP)
* Designed for academic use

---

## Learning Objectives

This project demonstrates:

* TCP socket programming in C
* Client–server architecture
* Turn synchronization
* Protocol design
* Memory management
* Deterministic AI logic
* Optional AI/LLM integration
* Debugging distributed programs

---

## License

This project is for **educational purposes**.
Reuse is allowed with attribution.

---

## Author

**Adil Hamidi**
Computer Science / Systems & Networks
France
