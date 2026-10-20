import express from 'express';
import { createServer } from 'http';
import { WebSocketServer, WebSocket } from 'ws';
import fs from 'fs';
import path from 'path';
import crypto from 'crypto';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
app.use(express.json());

const PORT = process.env.PORT || 3000;
const USERS_FILE = path.join(__dirname, 'users.txt');

// ============================================================================
// SECURITY: Cryptographically Hardened Password Hashing & Salt Generation (scrypt)
// ============================================================================
function hashPassword(password: string, salt: string): string {
  return crypto.scryptSync(password, salt, 64).toString('hex');
}

function generateSalt(): string {
  return crypto.randomBytes(16).toString('hex');
}

function verifyPassword(password: string, salt: string, storedHash: string): boolean {
  try {
    const computedHash = crypto.scryptSync(password, salt, 64).toString('hex');
    const a = Buffer.from(computedHash, 'hex');
    const b = Buffer.from(storedHash, 'hex');
    if (a.length !== b.length) return false;
    return crypto.timingSafeEqual(a, b);
  } catch {
    return false;
  }
}

interface UserRecord {
  username: string;
  salt: string;
  passwordHash: string;
}

// In-Memory User Store (Synced with users.txt)
const userStore = new Map<string, UserRecord>();

function loadUsersFromFile() {
  try {
    if (!fs.existsSync(USERS_FILE)) {
      fs.writeFileSync(
        USERS_FILE,
        '# CS204 Database: username:salt_hex:scrypt_hash_hex\n',
        'utf8'
      );
    }
    const content = fs.readFileSync(USERS_FILE, 'utf8');
    const lines = content.split('\n');
    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed || trimmed.startsWith('#')) continue;
      const parts = trimmed.split(':');
      if (parts.length >= 3) {
        userStore.set(parts[0], {
          username: parts[0],
          salt: parts[1],
          passwordHash: parts[2],
        });
      }
    }
    console.log(`[AuthManager] Loaded ${userStore.size} persistent user records from users.txt`);
  } catch (err) {
    console.error('[AuthManager ERROR] Error loading users.txt:', err);
  }
}

function appendUserToFile(record: UserRecord): boolean {
  try {
    const line = `${record.username}:${record.salt}:${record.passwordHash}\n`;
    fs.appendFileSync(USERS_FILE, line, 'utf8');
    return true;
  } catch (err) {
    console.error('[AuthManager ERROR] Failed writing to users.txt:', err);
    return false;
  }
}

loadUsersFromFile();

// ============================================================================
// SECURITY: Session Token Management (Cryptographically Random CSPRNG Tokens)
// ============================================================================
interface SessionData {
  username: string;
  createdAt: number;
}
const sessionTokens = new Map<string, SessionData>();

function issueSessionToken(username: string): string {
  const token = crypto.randomBytes(32).toString('hex');
  sessionTokens.set(token, { username, createdAt: Date.now() });
  return token;
}

function validateSessionToken(token: string): string | null {
  if (!token || typeof token !== 'string') return null;
  const session = sessionTokens.get(token);
  if (!session) return null;
  // 7-day session lifetime
  if (Date.now() - session.createdAt > 7 * 24 * 60 * 60 * 1000) {
    sessionTokens.delete(token);
    return null;
  }
  return session.username;
}

// ============================================================================
// REST API ENDPOINTS
// ============================================================================

// Register new user
app.post('/api/register', (req, res) => {
  const { username, password } = req.body;
  if (!username || !password) {
    return res.status(400).json({ success: false, message: 'Username and password are required.' });
  }

  const cleanUser = String(username).trim();
  if (cleanUser.length < 3 || cleanUser.length > 20) {
    return res.status(400).json({ success: false, message: 'Username must be between 3 and 20 characters.' });
  }
  if (!/^[a-zA-Z0-9_]+$/.test(cleanUser)) {
    return res.status(400).json({ success: false, message: 'Username may only contain letters, numbers, and underscores.' });
  }
  if (String(password).length < 6) {
    return res.status(400).json({ success: false, message: 'Password must be at least 6 characters long.' });
  }

  if (userStore.has(cleanUser)) {
    return res.status(409).json({ success: false, message: 'Username is already taken.' });
  }

  const salt = generateSalt();
  const hash = hashPassword(String(password), salt);
  const newRecord: UserRecord = { username: cleanUser, salt, passwordHash: hash };

  // Write to disk first (Write-Ahead durability guarantee)
  const writeSuccess = appendUserToFile(newRecord);
  if (!writeSuccess) {
    return res.status(500).json({ success: false, message: 'Internal server error: unable to save credentials to disk.' });
  }

  userStore.set(cleanUser, newRecord);
  const token = issueSessionToken(cleanUser);

  console.log(`[AuthManager] Registered and issued token for: ${cleanUser}`);
  return res.json({ success: true, message: 'Registration successful.', username: cleanUser, token });
});

// Login existing user
app.post('/api/login', (req, res) => {
  const { username, password } = req.body;
  if (!username || !password) {
    return res.status(400).json({ success: false, message: 'Invalid username or password.' });
  }

  const cleanUser = String(username).trim();
  const record = userStore.get(cleanUser);

  // Use timing-safe evaluation; generic error prevents user enumeration
  if (!record || !verifyPassword(String(password), record.salt, record.passwordHash)) {
    return res.status(401).json({ success: false, message: 'Invalid username or password.' });
  }

  const token = issueSessionToken(cleanUser);
  console.log(`[AuthManager] Authenticated user: ${cleanUser}`);
  return res.json({ success: true, message: 'Login successful.', username: cleanUser, token });
});

// Validate existing session token (for page refresh / multi-device resume)
app.get('/api/session', (req, res) => {
  const authHeader = req.headers.authorization;
  const token = authHeader?.startsWith('Bearer ') ? authHeader.slice(7) : (req.query.token as string);

  const username = validateSessionToken(token);
  if (!username) {
    return res.status(401).json({ success: false, message: 'Invalid or expired session token.' });
  }

  return res.json({ success: true, username });
});

// Logout
app.post('/api/logout', (req, res) => {
  const authHeader = req.headers.authorization;
  const token = authHeader?.startsWith('Bearer ') ? authHeader.slice(7) : (req.body.token as string);
  if (token) {
    sessionTokens.delete(token);
  }
  return res.json({ success: true });
});

// Endpoint to inspect C++ source files (safe list only, users.txt is NOT exposed)
app.get('/api/cpp/files', (_req, res) => {
  const safeFilePaths = [
    'include/AuthManager.h',
    'include/Board.h',
    'include/Player.h',
    'include/AIPlayer.h',
    'include/WebSocketPlayer.h',
    'include/LocalPlayer.h',
    'include/GameSession.h',
    'include/ServerNetworkManager.h',
    'src/AuthManager.cpp',
    'src/Board.cpp',
    'src/Player.cpp',
    'src/AIPlayer.cpp',
    'src/WebSocketPlayer.cpp',
    'src/LocalPlayer.cpp',
    'src/GameSession.cpp',
    'src/ServerNetworkManager.cpp',
    'src/main.cpp',
    'Makefile',
    'CMakeLists.txt',
    'build_linux.sh'
  ];

  const files = safeFilePaths.map(relPath => {
    const fullPath = path.join(__dirname, relPath);
    let content = '';
    let exists = false;
    if (fs.existsSync(fullPath)) {
      content = fs.readFileSync(fullPath, 'utf8');
      exists = true;
    }
    return { path: relPath, content, exists };
  });

  res.json({ files });
});

// Explicit 404 handler for unknown API routes
app.all('/api/*', (_req, res) => {
  res.status(404).json({ success: false, message: 'API endpoint not found.' });
});

// ============================================================================
// GAME LOGIC: Server-Authoritative Multi-Device Room & Match Management
// ============================================================================
class WebGameSession {
  sessionId: string;
  grid: string[][]; // 3x3 array initialized to ' '
  playerX: string;
  playerO: string | null;
  currentTurn: 'X' | 'O';
  state: 'WAITING' | 'ACTIVE' | 'FINISHED' | 'ABANDONED';
  winner: string;
  moveCount: number;
  isBot: boolean;

  // Mutual Rematch Agreement
  rematchRequestedBy: Set<string>;

  // Disconnection recovery tracking
  disconnectedPlayer: string | null;
  disconnectTimer: NodeJS.Timeout | null;

  constructor(sessionId: string, hostUser: string, isBot = false) {
    this.sessionId = sessionId;
    this.grid = [
      [' ', ' ', ' '],
      [' ', ' ', ' '],
      [' ', ' ', ' '],
    ];
    this.playerX = hostUser;
    this.isBot = isBot;
    this.playerO = isBot ? 'Minimax AI (Bot)' : null;
    this.currentTurn = 'X';
    this.state = isBot ? 'ACTIVE' : 'WAITING';
    this.winner = '';
    this.moveCount = 0;
    this.rematchRequestedBy = new Set();
    this.disconnectedPlayer = null;
    this.disconnectTimer = null;
  }

  joinGuest(guestUser: string): boolean {
    if (this.state !== 'WAITING' || this.playerO !== null) return false;
    if (this.playerX === guestUser) return false; // Prohibit self-joining

    this.playerO = guestUser;
    this.state = 'ACTIVE';
    this.currentTurn = 'X';
    this.rematchRequestedBy.clear();
    return true;
  }

  makeMove(username: string, row: number, col: number): { success: boolean; message?: string } {
    if (this.state !== 'ACTIVE') {
      return { success: false, message: 'Game is not currently active.' };
    }

    const symbol: 'X' | 'O' = username === this.playerX ? 'X' : (username === this.playerO ? 'O' : null as any);
    if (!symbol) {
      return { success: false, message: 'You are not a player in this match.' };
    }

    if (symbol !== this.currentTurn) {
      return { success: false, message: 'Not your turn. Please wait for your opponent.' };
    }

    if (row < 0 || row > 2 || col < 0 || col > 2) {
      return { success: false, message: 'Coordinate out of bounds.' };
    }

    if (this.grid[row][col] !== ' ') {
      return { success: false, message: 'Cell is already occupied.' };
    }

    this.grid[row][col] = symbol;
    this.moveCount++;

    // Evaluate Win or Draw
    if (this.checkWin(symbol)) {
      this.state = 'FINISHED';
      this.winner = username;
    } else if (this.moveCount >= 9) {
      this.state = 'FINISHED';
      this.winner = 'DRAW';
    } else {
      this.currentTurn = this.currentTurn === 'X' ? 'O' : 'X';
    }

    return { success: true };
  }

  private checkWin(sym: string): boolean {
    const g = this.grid;
    for (let r = 0; r < 3; r++) {
      if (g[r][0] === sym && g[r][1] === sym && g[r][2] === sym) return true;
    }
    for (let c = 0; c < 3; c++) {
      if (g[0][c] === sym && g[1][c] === sym && g[2][c] === sym) return true;
    }
    if (g[0][0] === sym && g[1][1] === sym && g[2][2] === sym) return true;
    if (g[0][2] === sym && g[1][1] === sym && g[2][0] === sym) return true;
    return false;
  }

  // =========================================================================
  // MINIMAX ALGORITHM (Optimal Game-Tree Depth First Search)
  // =========================================================================
  private minimax(grid: string[][], depth: number, isMaximizing: boolean): number {
    if (this.checkWinFor(grid, 'O')) return 10 - depth;
    if (this.checkWinFor(grid, 'X')) return depth - 10;
    if (this.isBoardFull(grid)) return 0;

    if (isMaximizing) {
      let maxEval = -Infinity;
      for (let r = 0; r < 3; r++) {
        for (let c = 0; c < 3; c++) {
          if (grid[r][c] === ' ') {
            grid[r][c] = 'O';
            const evaluation = this.minimax(grid, depth + 1, false);
            grid[r][c] = ' ';
            maxEval = Math.max(maxEval, evaluation);
          }
        }
      }
      return maxEval;
    } else {
      let minEval = Infinity;
      for (let r = 0; r < 3; r++) {
        for (let c = 0; c < 3; c++) {
          if (grid[r][c] === ' ') {
            grid[r][c] = 'X';
            const evaluation = this.minimax(grid, depth + 1, true);
            grid[r][c] = ' ';
            minEval = Math.min(minEval, evaluation);
          }
        }
      }
      return minEval;
    }
  }

  private isBoardFull(grid: string[][]): boolean {
    for (let r = 0; r < 3; r++) {
      for (let c = 0; c < 3; c++) {
        if (grid[r][c] === ' ') return false;
      }
    }
    return true;
  }

  private checkWinFor(grid: string[][], sym: string): boolean {
    for (let r = 0; r < 3; r++) {
      if (grid[r][0] === sym && grid[r][1] === sym && grid[r][2] === sym) return true;
    }
    for (let c = 0; c < 3; c++) {
      if (grid[0][c] === sym && grid[1][c] === sym && grid[2][c] === sym) return true;
    }
    if (grid[0][0] === sym && grid[1][1] === sym && grid[2][2] === sym) return true;
    if (grid[0][2] === sym && grid[1][1] === sym && grid[2][0] === sym) return true;
    return false;
  }

  getBestBotMove(): [number, number] {
    let bestScore = -Infinity;
    let bestMove: [number, number] = [-1, -1];

    if (this.grid[1][1] === ' ') {
      let emptyCount = 0;
      for (let r = 0; r < 3; r++) {
        for (let c = 0; c < 3; c++) {
          if (this.grid[r][c] === ' ') emptyCount++;
        }
      }
      if (emptyCount >= 8) return [1, 1];
    }

    for (let r = 0; r < 3; r++) {
      for (let c = 0; c < 3; c++) {
        if (this.grid[r][c] === ' ') {
          this.grid[r][c] = 'O';
          const score = this.minimax(this.grid, 0, false);
          this.grid[r][c] = ' ';
          if (score > bestScore) {
            bestScore = score;
            bestMove = [r, c];
          }
        }
      }
    }
    return bestMove;
  }

  restart() {
    this.grid = [
      [' ', ' ', ' '],
      [' ', ' ', ' '],
      [' ', ' ', ' '],
    ];
    this.currentTurn = 'X';
    this.state = 'ACTIVE';
    this.winner = '';
    this.moveCount = 0;
    this.rematchRequestedBy.clear();
  }

  serialize(): string {
    return this.grid.flat().join('');
  }

  toJson() {
    return {
      type: 'game_state',
      sessionId: this.sessionId,
      state: this.state,
      playerX: this.playerX,
      playerO: this.playerO || '',
      currentTurn: this.currentTurn,
      winner: this.winner,
      board: this.serialize(),
      isBot: this.isBot,
      rematchRequestedBy: Array.from(this.rematchRequestedBy),
      disconnectedPlayer: this.disconnectedPlayer,
    };
  }
}

// Global active games & socket connections
const activeRooms = new Map<string, WebGameSession>();
const userSockets = new Map<string, WebSocket>();
const userToRoom = new Map<string, string>();

// Generate collision-resistant 6-character room codes (e.g., "7KPX92")
function generateUniqueRoomCode(prefix = 'ROOM'): string {
  let code = '';
  do {
    const randHex = crypto.randomBytes(3).toString('hex').toUpperCase();
    code = `${prefix}-${randHex}`;
  } while (activeRooms.has(code));
  return code;
}

// Clean up player membership from previous room (Enforces 1 Active Room Invariant)
function cleanupPlayerFromRoom(username: string, roomId: string) {
  const session = activeRooms.get(roomId);
  if (!session) {
    userToRoom.delete(username);
    return;
  }

  if (session.state === 'WAITING' && session.playerX === username) {
    // Delete unstarted waiting room if host leaves
    activeRooms.delete(roomId);
  } else if (session.state === 'ACTIVE') {
    // Player forfeited by leaving active room
    session.state = 'ABANDONED';
    session.winner = username === session.playerX
      ? (session.playerO ? `${session.playerO} (by forfeit)` : 'ABANDONED')
      : `${session.playerX} (by forfeit)`;
    broadcastRoom(roomId);
  }

  userToRoom.delete(username);
  broadcastLobbyRooms();
}

// ============================================================================
// HTTP & WEBSOCKET SERVER ATTACHMENT
// ============================================================================
const server = createServer(app);
const wss = new WebSocketServer({ server, path: '/ws' });

function broadcastRoom(sessionId: string) {
  const room = activeRooms.get(sessionId);
  if (!room) return;
  const payload = JSON.stringify(room.toJson());

  if (room.playerX && userSockets.has(room.playerX)) {
    const ws = userSockets.get(room.playerX)!;
    if (ws.readyState === WebSocket.OPEN) ws.send(payload);
  }
  if (room.playerO && userSockets.has(room.playerO)) {
    const ws = userSockets.get(room.playerO)!;
    if (ws.readyState === WebSocket.OPEN) ws.send(payload);
  }
}

function broadcastLobbyRooms() {
  const roomsList = Array.from(activeRooms.values())
    .filter(r => !r.isBot && (r.state === 'WAITING' || r.state === 'ACTIVE'))
    .map(r => ({
      sessionId: r.sessionId,
      state: r.state,
      playerX: r.playerX,
      playerO: r.playerO || '',
    }));

  const payload = JSON.stringify({ type: 'lobby_list', rooms: roomsList });
  for (const client of wss.clients) {
    if (client.readyState === WebSocket.OPEN && (client as any).authenticatedUser) {
      client.send(payload);
    }
  }
}

wss.on('connection', (ws) => {
  console.log('[WebSocket] Connection received from network client');

  ws.on('message', (raw) => {
    try {
      const data = JSON.parse(raw.toString());
      const { type } = data;

      // -------------------------------------------------------------
      // 1. AUTHENTICATION HANDSHAKE (Token Required)
      // -------------------------------------------------------------
      if (type === 'auth') {
        const token = data.token;
        const authenticatedUsername = validateSessionToken(token);

        if (!authenticatedUsername) {
          ws.send(JSON.stringify({ type: 'auth_error', message: 'Invalid or expired session token. Please log in.' }));
          ws.close();
          return;
        }

        // Clean up previous socket if user connected from another tab
        const existingSocket = userSockets.get(authenticatedUsername);
        if (existingSocket && existingSocket !== ws && existingSocket.readyState === WebSocket.OPEN) {
          existingSocket.send(JSON.stringify({ type: 'session_replaced', message: 'Connected from another tab or window.' }));
          existingSocket.close();
        }

        // Bind authenticated identity to this socket
        (ws as any).authenticatedUser = authenticatedUsername;
        userSockets.set(authenticatedUsername, ws);

        ws.send(JSON.stringify({ type: 'auth_success', username: authenticatedUsername }));
        console.log(`[WebSocket] Bound authenticated socket to user: ${authenticatedUsername}`);

        // Check if user has an active room to resume (Multi-Device Reconnection Flow)
        const currentRoomId = userToRoom.get(authenticatedUsername);
        if (currentRoomId && activeRooms.has(currentRoomId)) {
          const session = activeRooms.get(currentRoomId)!;
          if (session.disconnectTimer) {
            clearTimeout(session.disconnectTimer);
            session.disconnectTimer = null;
          }
          if (session.disconnectedPlayer === authenticatedUsername) {
            session.disconnectedPlayer = null;
          }
          console.log(`[Reconnection] ${authenticatedUsername} reconnected to active room ${currentRoomId}`);
          broadcastRoom(currentRoomId);
        }

        broadcastLobbyRooms();
        return;
      }

      // -------------------------------------------------------------
      // SECURITY GUARD: All subsequent actions require authenticated socket
      // -------------------------------------------------------------
      const authUser: string = (ws as any).authenticatedUser;
      if (!authUser) {
        ws.send(JSON.stringify({ type: 'error', message: 'Unauthorized. You must provide a valid session token.' }));
        return;
      }

      // -------------------------------------------------------------
      // 2. CREATE MULTIPLAYER ROOM (PvP)
      // -------------------------------------------------------------
      if (type === 'create_room') {
        // Enforce 1 active room invariant
        const oldRoom = userToRoom.get(authUser);
        if (oldRoom) {
          cleanupPlayerFromRoom(authUser, oldRoom);
        }

        const roomId = generateUniqueRoomCode('ROOM');
        const session = new WebGameSession(roomId, authUser, false);
        activeRooms.set(roomId, session);
        userToRoom.set(authUser, roomId);

        console.log(`[Lobby] PvP Room ${roomId} created by ${authUser}`);
        ws.send(JSON.stringify(session.toJson()));
        broadcastLobbyRooms();
      }

      // -------------------------------------------------------------
      // 3. CREATE BOT ROOM (Single-Player Minimax)
      // -------------------------------------------------------------
      else if (type === 'create_bot_room') {
        const oldRoom = userToRoom.get(authUser);
        if (oldRoom) {
          cleanupPlayerFromRoom(authUser, oldRoom);
        }

        const roomId = generateUniqueRoomCode('BOT');
        const session = new WebGameSession(roomId, authUser, true);
        activeRooms.set(roomId, session);
        userToRoom.set(authUser, roomId);

        console.log(`[Lobby] Minimax Bot Room ${roomId} created for ${authUser}`);
        ws.send(JSON.stringify(session.toJson()));
      }

      // -------------------------------------------------------------
      // 4. JOIN ROOM
      // -------------------------------------------------------------
      else if (type === 'join_room') {
        const { roomId } = data;
        const cleanRoomId = String(roomId || '').trim().toUpperCase();

        const session = activeRooms.get(cleanRoomId);
        if (!session) {
          return ws.send(JSON.stringify({ type: 'error', message: 'Room code does not exist.' }));
        }

        // Prevent joining own room
        if (session.playerX === authUser) {
          return ws.send(JSON.stringify({ type: 'error', message: 'You are already the host of this room.' }));
        }

        // Leave any prior room
        const oldRoom = userToRoom.get(authUser);
        if (oldRoom && oldRoom !== cleanRoomId) {
          cleanupPlayerFromRoom(authUser, oldRoom);
        }

        if (!session.joinGuest(authUser)) {
          return ws.send(JSON.stringify({ type: 'error', message: 'Room is already full or in progress.' }));
        }

        userToRoom.set(authUser, cleanRoomId);
        console.log(`[Lobby] ${authUser} joined room ${cleanRoomId}`);
        broadcastRoom(cleanRoomId);
        broadcastLobbyRooms();
      }

      // -------------------------------------------------------------
      // 5. SUBMIT MOVE
      // -------------------------------------------------------------
      else if (type === 'move') {
        const { row, col } = data;
        const roomId = userToRoom.get(authUser);
        if (!roomId) {
          return ws.send(JSON.stringify({ type: 'error', message: 'You are not currently in a room.' }));
        }

        const session = activeRooms.get(roomId);
        if (!session) return;

        // Authoritative move validation
        const result = session.makeMove(authUser, Number(row), Number(col));
        if (!result.success) {
          return ws.send(JSON.stringify({ type: 'move_rejected', message: result.message }));
        }

        broadcastRoom(roomId);

        if (session.state === 'FINISHED') {
          broadcastLobbyRooms();
        } else if (session.isBot && session.state === 'ACTIVE' && session.currentTurn === 'O') {
          // Guarded Minimax Bot execution (race-condition protected)
          const expectedMoveCount = session.moveCount;
          setTimeout(() => {
            const current = activeRooms.get(roomId);
            if (
              current &&
              current.state === 'ACTIVE' &&
              current.currentTurn === 'O' &&
              current.moveCount === expectedMoveCount
            ) {
              const [botR, botC] = current.getBestBotMove();
              if (botR !== -1 && botC !== -1) {
                current.makeMove('Minimax AI (Bot)', botR, botC);
                broadcastRoom(roomId);
                if ((current.state as string) === 'FINISHED') {
                  broadcastLobbyRooms();
                }
              }
            }
          }, 350);
        }
      }

      // -------------------------------------------------------------
      // 6. MUTUAL REMATCH AGREEMENT
      // -------------------------------------------------------------
      else if (type === 'rematch') {
        const roomId = userToRoom.get(authUser);
        if (!roomId) return;
        const session = activeRooms.get(roomId);
        if (!session || session.state !== 'FINISHED') return;

        if (session.isBot) {
          // Bot immediately consents
          session.restart();
          broadcastRoom(roomId);
        } else {
          session.rematchRequestedBy.add(authUser);

          // Require both players to consent
          if (session.rematchRequestedBy.size >= 2) {
            session.restart();
            broadcastRoom(roomId);
          } else {
            // Broadcast that one player wants a rematch
            broadcastRoom(roomId);
          }
        }
      }

      // -------------------------------------------------------------
      // 7. LEAVE ROOM
      // -------------------------------------------------------------
      else if (type === 'leave_room') {
        const roomId = userToRoom.get(authUser);
        if (roomId) {
          cleanupPlayerFromRoom(authUser, roomId);
        }
      }

      // -------------------------------------------------------------
      // 8. LOBBY REFRESH
      // -------------------------------------------------------------
      else if (type === 'get_lobby') {
        broadcastLobbyRooms();
      }

    } catch (err) {
      console.error('[WebSocket ERROR] Malformed frame payload:', err);
    }
  });

  // -----------------------------------------------------------------
  // MULTI-DEVICE RECONNECTION & DISCONNECTION RECOVERY
  // -----------------------------------------------------------------
  ws.on('close', () => {
    const authUser = (ws as any).authenticatedUser;
    if (!authUser) return;

    // Only clean up if this was the active socket registered for this user
    if (userSockets.get(authUser) === ws) {
      userSockets.delete(authUser);
    }

    const roomId = userToRoom.get(authUser);
    if (!roomId) return;

    const session = activeRooms.get(roomId);
    if (!session) return;

    if (session.isBot) {
      // In solo bot mode, clean up room immediately on disconnect
      activeRooms.delete(roomId);
      userToRoom.delete(authUser);
    } else if (session.state === 'WAITING') {
      // Host disconnected while waiting for opponent: 30s grace period
      session.disconnectTimer = setTimeout(() => {
        if (!userSockets.has(authUser) && activeRooms.get(roomId)?.state === 'WAITING') {
          activeRooms.delete(roomId);
          userToRoom.delete(authUser);
          broadcastLobbyRooms();
          console.log(`[Lobby] Cleaned up abandoned waiting room ${roomId}`);
        }
      }, 30000);
    } else if (session.state === 'ACTIVE') {
      // Human-vs-Human match in progress: 45s grace period before forfeit
      session.disconnectedPlayer = authUser;
      broadcastRoom(roomId);

      console.log(`[Disconnection] ${authUser} disconnected from active room ${roomId}. 45s grace period started.`);

      session.disconnectTimer = setTimeout(() => {
        const currentSession = activeRooms.get(roomId);
        if (currentSession && currentSession.state === 'ACTIVE' && currentSession.disconnectedPlayer === authUser) {
          currentSession.state = 'ABANDONED';
          currentSession.winner = authUser === currentSession.playerX
            ? `${currentSession.playerO} (by forfeit)`
            : `${currentSession.playerX} (by forfeit)`;
          broadcastRoom(roomId);
          console.log(`[Forfeit] Grace period expired for ${authUser}. ${currentSession.winner}`);
        }
      }, 45000);
    }
  });
});

// ============================================================================
// VITE MIDDLEWARE / PRODUCTION STATIC ASSETS
// ============================================================================
async function startServer() {
  if (process.env.NODE_ENV !== 'production') {
    const { createServer: createViteServer } = await import('vite');
    const vite = await createViteServer({
      server: { middlewareMode: true },
      appType: 'spa',
    });
    app.use(vite.middlewares);
  } else {
    const distPath = path.join(__dirname, 'dist');
    app.use(express.static(distPath));
    app.get('*', (_req, res) => {
      res.sendFile(path.join(distPath, 'index.html'));
    });
  }

  server.listen(PORT, () => {
    console.log(`[SERVER BOOT] Production Tic-Tac-Toe Server running on port ${PORT}`);
    console.log(`[WEBSOCKET] Secure WebSocket endpoint live at ws://localhost:${PORT}/ws`);
  });
}

startServer();
