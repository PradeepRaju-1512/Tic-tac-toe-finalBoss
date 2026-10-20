/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState, useEffect, useRef, useCallback } from 'react';
import {
  Shield,
  Cpu,
  Layers,
  Terminal,
  Users,
  LogOut,
  RefreshCw,
  Copy,
  Check,
  FileCode,
  Lock,
  AlertCircle,
  Swords,
  Award,
  Wifi,
  WifiOff,
  UserCheck
} from 'lucide-react';

// Web Audio sound generator for crisp sensory feedback
function playTone(type: 'move' | 'win' | 'draw' | 'error') {
  try {
    const ctx = new (window.AudioContext || (window as any).webkitAudioContext)();
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.connect(gain);
    gain.connect(ctx.destination);

    const now = ctx.currentTime;
    if (type === 'move') {
      osc.type = 'sine';
      osc.frequency.setValueAtTime(440, now);
      osc.frequency.exponentialRampToValueAtTime(880, now + 0.08);
      gain.gain.setValueAtTime(0.12, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.08);
      osc.start(now);
      osc.stop(now + 0.08);
    } else if (type === 'win') {
      osc.type = 'triangle';
      osc.frequency.setValueAtTime(523.25, now);
      osc.frequency.setValueAtTime(659.25, now + 0.1);
      osc.frequency.setValueAtTime(783.99, now + 0.2);
      gain.gain.setValueAtTime(0.15, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.35);
      osc.start(now);
      osc.stop(now + 0.35);
    } else if (type === 'draw') {
      osc.type = 'sawtooth';
      osc.frequency.setValueAtTime(300, now);
      osc.frequency.linearRampToValueAtTime(200, now + 0.2);
      gain.gain.setValueAtTime(0.08, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.2);
      osc.start(now);
      osc.stop(now + 0.2);
    } else if (type === 'error') {
      osc.type = 'square';
      osc.frequency.setValueAtTime(150, now);
      gain.gain.setValueAtTime(0.1, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.15);
      osc.start(now);
      osc.stop(now + 0.15);
    }
  } catch {
    // AudioContext blocked
  }
}

interface GameState {
  sessionId: string;
  state: 'WAITING' | 'ACTIVE' | 'FINISHED' | 'ABANDONED';
  playerX: string;
  playerO: string;
  currentTurn: 'X' | 'O';
  winner: string;
  board: string;
  isBot?: boolean;
  rematchRequestedBy?: string[];
  disconnectedPlayer?: string | null;
}

interface LobbyRoom {
  sessionId: string;
  state: string;
  playerX: string;
  playerO: string;
}

interface CppFile {
  path: string;
  content: string;
  exists: boolean;
}

const STORAGE_KEY = 'tictactoe_session_token';

export default function App() {
  // Navigation
  const [activeTab, setActiveTab] = useState<'game' | 'code'>('game');

  // Authentication State
  const [user, setUser] = useState<string | null>(null);
  const [sessionToken, setSessionToken] = useState<string | null>(() => localStorage.getItem(STORAGE_KEY));
  const [authMode, setAuthMode] = useState<'login' | 'register'>('login');
  const [authUsername, setAuthUsername] = useState('');
  const [authPassword, setAuthPassword] = useState('');
  const [authError, setAuthError] = useState<string | null>(null);
  const [authSuccess, setAuthSuccess] = useState<string | null>(null);
  const [authLoading, setAuthLoading] = useState(false);

  // WebSocket & Multiplayer State
  const [connected, setConnected] = useState(false);
  const [reconnecting, setReconnecting] = useState(false);
  const [lobbyRooms, setLobbyRooms] = useState<LobbyRoom[]>([]);
  const [currentGame, setCurrentGame] = useState<GameState | null>(null);
  const [inputRoomCode, setInputRoomCode] = useState('');
  const [gameMessage, setGameMessage] = useState<string | null>(null);
  const [copiedCode, setCopiedCode] = useState(false);

  // Inspector
  const [cppFiles, setCppFiles] = useState<CppFile[]>([]);
  const [selectedFilePath, setSelectedFilePath] = useState<string>('include/AuthManager.h');
  const [loadingFiles, setLoadingFiles] = useState(false);

  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimeoutRef = useRef<NodeJS.Timeout | null>(null);

  // Validate existing session token on mount (Multi-Device Refresh Resilience)
  useEffect(() => {
    async function restoreSession() {
      if (!sessionToken) return;
      try {
        const res = await fetch('/api/session', {
          headers: { Authorization: `Bearer ${sessionToken}` },
        });
        const data = await res.json();
        if (res.ok && data.success) {
          setUser(data.username);
        } else {
          localStorage.removeItem(STORAGE_KEY);
          setSessionToken(null);
          setUser(null);
        }
      } catch {
        // Offline / server unreachable
      }
    }
    restoreSession();
  }, [sessionToken]);

  // Connect WebSocket with Automatic Exponential Reconnect
  const connectWebSocket = useCallback(() => {
    if (wsRef.current) {
      wsRef.current.close();
    }

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.host;
    const wsUrl = `${protocol}//${host}/ws`;

    const socket = new WebSocket(wsUrl);

    socket.onopen = () => {
      setConnected(true);
      setReconnecting(false);

      // Authenticate with verified cryptographic token
      const token = localStorage.getItem(STORAGE_KEY);
      if (token) {
        socket.send(JSON.stringify({ type: 'auth', token }));
      }
      socket.send(JSON.stringify({ type: 'get_lobby' }));
    };

    socket.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        if (msg.type === 'auth_success') {
          setConnected(true);
          setUser(msg.username);
        } else if (msg.type === 'auth_error') {
          setAuthError(msg.message);
          localStorage.removeItem(STORAGE_KEY);
          setSessionToken(null);
          setUser(null);
        } else if (msg.type === 'session_replaced') {
          setGameMessage('Session opened in another window. Disconnected.');
        } else if (msg.type === 'lobby_list') {
          setLobbyRooms(msg.rooms || []);
        } else if (msg.type === 'game_state') {
          setCurrentGame(msg);
          setGameMessage(null);
          if (msg.state === 'FINISHED') {
            if (msg.winner === 'DRAW') playTone('draw');
            else playTone('win');
          }
        } else if (msg.type === 'move_rejected' || msg.type === 'error') {
          setGameMessage(msg.message);
          playTone('error');
        }
      } catch (err) {
        console.error('WebSocket parsing error:', err);
      }
    };

    socket.onclose = () => {
      setConnected(false);
      // Auto-reconnect every 3 seconds if user is logged in
      if (localStorage.getItem(STORAGE_KEY)) {
        setReconnecting(true);
        reconnectTimeoutRef.current = setTimeout(() => {
          connectWebSocket();
        }, 3000);
      }
    };

    wsRef.current = socket;
  }, []);

  useEffect(() => {
    if (user && sessionToken) {
      connectWebSocket();
    }
    return () => {
      if (reconnectTimeoutRef.current) clearTimeout(reconnectTimeoutRef.current);
      if (wsRef.current) wsRef.current.close();
    };
  }, [user, sessionToken, connectWebSocket]);

  // Load C++ code files for inspector
  useEffect(() => {
    async function loadCppFiles() {
      setLoadingFiles(true);
      try {
        const res = await fetch('/api/cpp/files');
        const data = await res.json();
        if (data.files) {
          setCppFiles(data.files);
        }
      } catch (err) {
        console.error('Failed to load C++ files:', err);
      } finally {
        setLoadingFiles(false);
      }
    }
    loadCppFiles();
  }, []);

  // Authentication Submission (Register / Login)
  const handleAuth = async (e: React.FormEvent) => {
    e.preventDefault();
    setAuthError(null);
    setAuthSuccess(null);
    setAuthLoading(true);

    const endpoint = authMode === 'login' ? '/api/login' : '/api/register';
    try {
      const res = await fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username: authUsername.trim(), password: authPassword }),
      });
      const data = await res.json();

      if (res.ok && data.success) {
        localStorage.setItem(STORAGE_KEY, data.token);
        setSessionToken(data.token);
        setUser(data.username);
        setAuthPassword('');
        setAuthError(null);

        // Authenticate socket
        if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
          wsRef.current.send(JSON.stringify({ type: 'auth', token: data.token }));
        }
      } else {
        setAuthError(data.message || 'Authentication failed.');
      }
    } catch {
      setAuthError('Connection error to authentication server.');
    } finally {
      setAuthLoading(false);
    }
  };

  const handleLogout = async () => {
    if (sessionToken) {
      try {
        await fetch('/api/logout', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ token: sessionToken }),
        });
      } catch {
        // Continue logout
      }
    }

    if (currentGame && wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({ type: 'leave_room' }));
    }

    localStorage.removeItem(STORAGE_KEY);
    setSessionToken(null);
    setUser(null);
    setCurrentGame(null);
    setAuthUsername('');
    setAuthPassword('');
    if (wsRef.current) wsRef.current.close();
  };

  // Game Room Actions
  const handleCreateRoom = () => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) return;
    setGameMessage(null);
    wsRef.current.send(JSON.stringify({ type: 'create_room' }));
  };

  const handleCreateBotRoom = () => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) return;
    setGameMessage(null);
    wsRef.current.send(JSON.stringify({ type: 'create_bot_room' }));
  };

  const handleJoinRoom = (roomId: string) => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN || !roomId) return;
    setGameMessage(null);
    wsRef.current.send(JSON.stringify({ type: 'join_room', roomId: roomId.trim().toUpperCase() }));
    setInputRoomCode('');
  };

  const handleCellClick = (r: number, c: number) => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN || !currentGame) return;
    if (currentGame.state !== 'ACTIVE') return;

    const cellIdx = r * 3 + c;
    if (currentGame.board[cellIdx] !== ' ') {
      playTone('error');
      return;
    }

    const isPlayerX = user === currentGame.playerX;
    const isMyTurn = (isPlayerX && currentGame.currentTurn === 'X') || (!isPlayerX && currentGame.currentTurn === 'O');
    if (!isMyTurn) {
      setGameMessage("Wait for your opponent's turn!");
      playTone('error');
      return;
    }

    playTone('move');
    wsRef.current.send(JSON.stringify({ type: 'move', row: r, col: c }));
  };

  const handleRematch = () => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) return;
    wsRef.current.send(JSON.stringify({ type: 'rematch' }));
  };

  const handleLeaveRoom = () => {
    if (!wsRef.current || wsRef.current.readyState !== WebSocket.OPEN) return;
    wsRef.current.send(JSON.stringify({ type: 'leave_room' }));
    setCurrentGame(null);
    setGameMessage(null);
  };

  const copyRoomCode = (code: string) => {
    navigator.clipboard.writeText(code);
    setCopiedCode(true);
    setTimeout(() => setCopiedCode(false), 2000);
  };

  const selectedFile = cppFiles.find(f => f.path === selectedFilePath);

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col font-sans selection:bg-cyan-500 selection:text-black">
      {/* Top Navigation Bar */}
      <header className="border-b border-slate-800 bg-slate-900/90 backdrop-blur sticky top-0 z-50">
        <div className="max-w-7xl mx-auto px-6 py-3.5 flex items-center justify-between gap-8">
          <div className="flex items-center gap-3 shrink-0">
            <span className="text-base font-semibold tracking-tight text-white whitespace-nowrap">
              C++ Systems Multiplayer
            </span>
            <span className="text-xs text-slate-400 font-mono hidden sm:inline">
              CS204 B.Tech Project
            </span>
          </div>

          <nav className="flex items-center gap-6 text-sm font-medium text-slate-400">
            <button
              onClick={() => setActiveTab('game')}
              className={`hover:text-white transition-colors whitespace-nowrap shrink-0 flex items-center gap-1.5 ${
                activeTab === 'game' ? 'text-cyan-400 border-b-2 border-cyan-400 py-1 font-semibold' : ''
              }`}
            >
              <Swords className="w-4 h-4" />
              <span>Game Arena</span>
            </button>
            <button
              onClick={() => setActiveTab('code')}
              className={`hover:text-white transition-colors whitespace-nowrap shrink-0 flex items-center gap-1.5 ${
                activeTab === 'code' ? 'text-cyan-400 border-b-2 border-cyan-400 py-1 font-semibold' : ''
              }`}
            >
              <FileCode className="w-4 h-4" />
              <span>OOP & C++ Inspector</span>
            </button>
          </nav>

          <div className="flex items-center gap-3 shrink-0">
            {user ? (
              <div className="flex items-center gap-3">
                <div className="flex items-center gap-1.5 text-xs text-slate-300 font-mono bg-slate-800/80 px-2.5 py-1 rounded border border-slate-700">
                  <UserCheck className="w-3.5 h-3.5 text-emerald-400" />
                  <span>{user}</span>
                </div>
                <button
                  onClick={handleLogout}
                  className="text-xs text-slate-400 hover:text-rose-400 transition-colors flex items-center gap-1 whitespace-nowrap"
                  title="Logout"
                >
                  <LogOut className="w-3.5 h-3.5" />
                  <span className="hidden sm:inline">Logout</span>
                </button>
              </div>
            ) : (
              <span className="text-xs text-slate-500 font-mono">Unauthenticated</span>
            )}
          </div>
        </div>
      </header>

      {/* Main Container */}
      <main className="flex-1 max-w-7xl w-full mx-auto p-6 md:p-8 flex flex-col">
        {/* VIEW 1: GAME ARENA */}
        {activeTab === 'game' && (
          <div className="flex-1 flex flex-col">
            {!user ? (
              /* Authentication Portal */
              <div className="max-w-md w-full mx-auto my-auto py-12">
                <div className="bg-slate-900 border border-slate-800 rounded-xl p-8 shadow-2xl">
                  <div className="mb-6 text-center">
                    <div className="w-12 h-12 bg-cyan-950 border border-cyan-800/60 rounded-xl flex items-center justify-center mx-auto mb-3">
                      <Lock className="w-6 h-6 text-cyan-400" />
                    </div>
                    <h1 className="text-xl font-bold text-white tracking-tight">
                      C++ Multi-Player Login
                    </h1>
                    <p className="text-xs text-slate-400 mt-1">
                      Hardened authentication with scrypt hashing & secure session tokens
                    </p>
                  </div>

                  <div className="flex bg-slate-950 p-1 rounded-lg mb-6 border border-slate-800">
                    <button
                      type="button"
                      onClick={() => { setAuthMode('login'); setAuthError(null); setAuthSuccess(null); }}
                      className={`flex-1 py-1.5 text-xs font-semibold rounded-md transition-colors ${
                        authMode === 'login' ? 'bg-cyan-500 text-slate-950 shadow' : 'text-slate-400 hover:text-white'
                      }`}
                    >
                      Login
                    </button>
                    <button
                      type="button"
                      onClick={() => { setAuthMode('register'); setAuthError(null); setAuthSuccess(null); }}
                      className={`flex-1 py-1.5 text-xs font-semibold rounded-md transition-colors ${
                        authMode === 'register' ? 'bg-cyan-500 text-slate-950 shadow' : 'text-slate-400 hover:text-white'
                      }`}
                    >
                      Register
                    </button>
                  </div>

                  {authError && (
                    <div className="mb-4 p-3 bg-rose-950/60 border border-rose-800 text-rose-300 text-xs rounded-lg flex items-center gap-2">
                      <AlertCircle className="w-4 h-4 shrink-0" />
                      <span>{authError}</span>
                    </div>
                  )}
                  {authSuccess && (
                    <div className="mb-4 p-3 bg-emerald-950/60 border border-emerald-800 text-emerald-300 text-xs rounded-lg flex items-center gap-2">
                      <Check className="w-4 h-4 shrink-0" />
                      <span>{authSuccess}</span>
                    </div>
                  )}

                  <form onSubmit={handleAuth} className="space-y-4">
                    <div>
                      <label className="block text-xs font-medium text-slate-300 mb-1">
                        Username
                      </label>
                      <input
                        type="text"
                        value={authUsername}
                        onChange={(e) => setAuthUsername(e.target.value)}
                        placeholder="e.g. student"
                        required
                        className="w-full px-3.5 py-2 bg-slate-950 border border-slate-700 rounded-lg text-sm text-white placeholder-slate-600 focus:outline-none focus:border-cyan-500 transition-colors"
                      />
                    </div>
                    <div>
                      <label className="block text-xs font-medium text-slate-300 mb-1">
                        Password (min 6 characters)
                      </label>
                      <input
                        type="password"
                        value={authPassword}
                        onChange={(e) => setAuthPassword(e.target.value)}
                        placeholder="••••••••"
                        required
                        minLength={6}
                        className="w-full px-3.5 py-2 bg-slate-950 border border-slate-700 rounded-lg text-sm text-white placeholder-slate-600 focus:outline-none focus:border-cyan-500 transition-colors"
                      />
                    </div>

                    <button
                      type="submit"
                      disabled={authLoading}
                      className="w-full mt-2 py-2.5 px-4 bg-cyan-500 hover:bg-cyan-400 disabled:opacity-50 text-slate-950 font-semibold text-sm rounded-lg transition-colors flex items-center justify-center gap-2 shadow-lg shadow-cyan-500/10"
                    >
                      {authLoading ? (
                        <RefreshCw className="w-4 h-4 animate-spin" />
                      ) : (
                        authMode === 'login' ? 'Authenticate & Enter' : 'Create Account'
                      )}
                    </button>
                  </form>

                  <div className="mt-6 pt-5 border-t border-slate-800 text-center">
                    <p className="text-xs text-slate-400 mb-2">Pre-registered demo accounts:</p>
                    <div className="flex justify-center gap-2">
                      <button
                        type="button"
                        onClick={() => { setAuthUsername('student'); setAuthPassword('pass1234'); }}
                        className="px-2.5 py-1 text-xs bg-slate-800 hover:bg-slate-700 text-slate-300 rounded border border-slate-700 transition-colors font-mono"
                      >
                        student / pass1234
                      </button>
                      <button
                        type="button"
                        onClick={() => { setAuthUsername('professor'); setAuthPassword('pass1234'); }}
                        className="px-2.5 py-1 text-xs bg-slate-800 hover:bg-slate-700 text-slate-300 rounded border border-slate-700 transition-colors font-mono"
                      >
                        professor / pass1234
                      </button>
                    </div>
                  </div>
                </div>
              </div>
            ) : !currentGame ? (
              /* Lobby & Matchmaking */
              <div className="grid grid-cols-1 lg:grid-cols-3 gap-8 my-auto py-6">
                <div className="lg:col-span-1 space-y-6">
                  {/* Single Player Bot Mode */}
                  <div className="bg-slate-900 border border-slate-800 rounded-xl p-6">
                    <h2 className="text-base font-semibold text-white mb-1 flex items-center gap-2">
                      <Cpu className="w-4 h-4 text-emerald-400" />
                      Single Player (Minimax AI)
                    </h2>
                    <p className="text-xs text-slate-400 mb-4">
                      Instant solo match against the server-side bot using recursive Minimax game-tree evaluation.
                    </p>
                    <button
                      onClick={handleCreateBotRoom}
                      className="w-full py-2.5 px-4 bg-emerald-500 hover:bg-emerald-400 text-slate-950 font-semibold text-sm rounded-lg transition-colors flex items-center justify-center gap-2 shadow-lg shadow-emerald-500/10"
                    >
                      <Cpu className="w-4 h-4" />
                      Play vs Bot
                    </button>
                  </div>

                  {/* Create PvP Match */}
                  <div className="bg-slate-900 border border-slate-800 rounded-xl p-6">
                    <h2 className="text-base font-semibold text-white mb-1 flex items-center gap-2">
                      <Swords className="w-4 h-4 text-cyan-400" />
                      Host Multiplayer Match
                    </h2>
                    <p className="text-xs text-slate-400 mb-4">
                      Create an authoritative room with collision-resistant 6-char code. You play as Player X.
                    </p>
                    <button
                      onClick={handleCreateRoom}
                      className="w-full py-2.5 px-4 bg-cyan-500 hover:bg-cyan-400 text-slate-950 font-semibold text-sm rounded-lg transition-colors flex items-center justify-center gap-2"
                    >
                      Create Game Room
                    </button>
                  </div>

                  {/* Join Match by Code */}
                  <div className="bg-slate-900 border border-slate-800 rounded-xl p-6">
                    <h2 className="text-base font-semibold text-white mb-1 flex items-center gap-2">
                      <Users className="w-4 h-4 text-cyan-400" />
                      Join by Code
                    </h2>
                    <p className="text-xs text-slate-400 mb-4">
                      Enter the room code provided by the host. You play as Player O.
                    </p>
                    <div className="flex gap-2">
                      <input
                        type="text"
                        value={inputRoomCode}
                        onChange={(e) => setInputRoomCode(e.target.value.toUpperCase())}
                        placeholder="ROOM-X8K9"
                        className="flex-1 px-3 py-2 bg-slate-950 border border-slate-700 rounded-lg text-sm text-white font-mono placeholder-slate-600 focus:outline-none focus:border-cyan-500 uppercase"
                      />
                      <button
                        onClick={() => handleJoinRoom(inputRoomCode)}
                        disabled={!inputRoomCode.trim()}
                        className="py-2 px-4 bg-slate-800 hover:bg-slate-700 disabled:opacity-40 text-white font-medium text-sm rounded-lg transition-colors"
                      >
                        Join
                      </button>
                    </div>
                  </div>
                </div>

                {/* Live Multiplayer Lobby */}
                <div className="lg:col-span-2 bg-slate-900 border border-slate-800 rounded-xl p-6 flex flex-col">
                  <div className="flex items-center justify-between pb-4 border-b border-slate-800">
                    <div>
                      <h2 className="text-base font-semibold text-white">Live Multiplayer Lobby</h2>
                      <p className="text-xs text-slate-400">Available human-vs-human rooms across devices</p>
                    </div>
                    <div className="flex items-center gap-2">
                      {connected ? (
                        <span className="flex items-center gap-1.5 text-xs text-emerald-400 font-mono bg-emerald-950/60 border border-emerald-800/80 px-2 py-0.5 rounded">
                          <Wifi className="w-3 h-3" /> Live
                        </span>
                      ) : (
                        <span className="flex items-center gap-1.5 text-xs text-rose-400 font-mono bg-rose-950/60 border border-rose-800/80 px-2 py-0.5 rounded">
                          <WifiOff className="w-3 h-3" /> {reconnecting ? 'Reconnecting...' : 'Offline'}
                        </span>
                      )}
                      <span className="text-xs font-mono text-cyan-400 bg-cyan-950/60 border border-cyan-800/80 px-2 py-0.5 rounded">
                        {lobbyRooms.length} Rooms
                      </span>
                    </div>
                  </div>

                  {gameMessage && (
                    <div className="mt-4 p-3 bg-rose-950/60 border border-rose-800 text-rose-300 text-xs rounded-lg flex items-center gap-2">
                      <AlertCircle className="w-4 h-4 shrink-0" />
                      <span>{gameMessage}</span>
                    </div>
                  )}

                  <div className="flex-1 mt-4 overflow-y-auto space-y-2.5">
                    {lobbyRooms.length === 0 ? (
                      <div className="h-48 flex flex-col items-center justify-center text-slate-500 text-xs">
                        <Users className="w-8 h-8 mb-2 opacity-40" />
                        <span>No waiting rooms currently. Click &quot;Create Game Room&quot; to host!</span>
                      </div>
                    ) : (
                      lobbyRooms.map((room) => (
                        <div
                          key={room.sessionId}
                          className="flex items-center justify-between p-3.5 bg-slate-950/70 border border-slate-800/80 rounded-lg hover:border-slate-700 transition-colors"
                        >
                          <div className="flex items-center gap-4">
                            <span className="text-xs font-mono font-bold text-white bg-slate-800 px-2.5 py-1 rounded">
                              {room.sessionId}
                            </span>
                            <div className="text-xs">
                              <span className="text-slate-400">Host:</span>{' '}
                              <span className="text-cyan-400 font-medium">{room.playerX}</span>
                              {room.playerO && (
                                <>
                                  <span className="text-slate-500 mx-1.5">vs</span>
                                  <span className="text-rose-400 font-medium">{room.playerO}</span>
                                </>
                              )}
                            </div>
                          </div>

                          <div className="flex items-center gap-3">
                            <span className="text-xs text-slate-400">
                              {room.state === 'WAITING' ? (
                                <span className="text-amber-400">Waiting for opponent</span>
                              ) : (
                                <span className="text-emerald-400">In Progress</span>
                              )}
                            </span>
                            {room.state === 'WAITING' && room.playerX !== user && (
                              <button
                                onClick={() => handleJoinRoom(room.sessionId)}
                                className="px-3 py-1 bg-cyan-500 hover:bg-cyan-400 text-slate-950 font-medium text-xs rounded transition-colors"
                              >
                                Join Match
                              </button>
                            )}
                          </div>
                        </div>
                      ))
                    )}
                  </div>
                </div>
              </div>
            ) : (
              /* Active Match View */
              <div className="max-w-2xl w-full mx-auto my-auto py-6">
                {/* HUD Header */}
                <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 mb-6">
                  <div className="flex items-center justify-between mb-4">
                    <div className="flex items-center gap-2">
                      <span className="text-xs font-mono font-bold text-slate-300 bg-slate-800 px-2.5 py-1 rounded">
                        {currentGame.sessionId}
                      </span>
                      <button
                        onClick={() => copyRoomCode(currentGame.sessionId)}
                        className="text-slate-400 hover:text-white transition-colors"
                        title="Copy Room Code"
                      >
                        {copiedCode ? <Check className="w-3.5 h-3.5 text-emerald-400" /> : <Copy className="w-3.5 h-3.5" />}
                      </button>
                      {currentGame.isBot && (
                        <span className="text-[10px] font-mono text-emerald-400 bg-emerald-950/80 border border-emerald-800 px-2 py-0.5 rounded">
                          Minimax Bot Match
                        </span>
                      )}
                    </div>

                    <button
                      onClick={handleLeaveRoom}
                      className="text-xs text-slate-400 hover:text-rose-400 transition-colors"
                    >
                      Leave Room
                    </button>
                  </div>

                  {/* Player Matchup Cards */}
                  <div className="grid grid-cols-2 gap-4">
                    {/* Player X */}
                    <div className={`p-3 rounded-lg border transition-all ${
                      currentGame.currentTurn === 'X' && currentGame.state === 'ACTIVE'
                        ? 'bg-cyan-950/40 border-cyan-500/60 shadow-lg shadow-cyan-500/10'
                        : 'bg-slate-950 border-slate-800'
                    }`}>
                      <div className="flex items-center justify-between">
                        <div className="flex items-center gap-2">
                          <span className="w-6 h-6 rounded bg-cyan-500/20 text-cyan-400 font-bold font-mono text-sm flex items-center justify-center">
                            X
                          </span>
                          <span className="text-sm font-semibold text-white truncate max-w-[120px]">
                            {currentGame.playerX}
                          </span>
                        </div>
                        {user === currentGame.playerX && (
                          <span className="text-[10px] uppercase font-mono text-slate-400 bg-slate-800 px-1.5 py-0.5 rounded">
                            You
                          </span>
                        )}
                      </div>
                      {currentGame.state === 'ACTIVE' && currentGame.currentTurn === 'X' && (
                        <div className="text-[11px] text-cyan-400 mt-1 font-medium">Turn Active</div>
                      )}
                    </div>

                    {/* Player O */}
                    <div className={`p-3 rounded-lg border transition-all ${
                      currentGame.currentTurn === 'O' && currentGame.state === 'ACTIVE'
                        ? 'bg-rose-950/40 border-rose-500/60 shadow-lg shadow-rose-500/10'
                        : 'bg-slate-950 border-slate-800'
                    }`}>
                      <div className="flex items-center justify-between">
                        <div className="flex items-center gap-2">
                          <span className="w-6 h-6 rounded bg-rose-500/20 text-rose-400 font-bold font-mono text-sm flex items-center justify-center">
                            O
                          </span>
                          <span className="text-sm font-semibold text-white truncate max-w-[120px]">
                            {currentGame.playerO || 'Waiting...'}
                          </span>
                        </div>
                        {user === currentGame.playerO && (
                          <span className="text-[10px] uppercase font-mono text-slate-400 bg-slate-800 px-1.5 py-0.5 rounded">
                            You
                          </span>
                        )}
                      </div>
                      {currentGame.state === 'ACTIVE' && currentGame.currentTurn === 'O' && (
                        <div className="text-[11px] text-rose-400 mt-1 font-medium">Turn Active</div>
                      )}
                    </div>
                  </div>
                </div>

                {/* Disconnection Grace Period Notice */}
                {currentGame.disconnectedPlayer && currentGame.state === 'ACTIVE' && (
                  <div className="mb-4 p-3.5 bg-amber-950/50 border border-amber-700/80 text-amber-300 text-xs rounded-xl flex items-center gap-2.5 animate-pulse">
                    <WifiOff className="w-4 h-4 shrink-0 text-amber-400" />
                    <span>
                      Player <strong>{currentGame.disconnectedPlayer}</strong> disconnected. 45-second reconnection grace period active before forfeit.
                    </span>
                  </div>
                )}

                {gameMessage && (
                  <div className="mb-4 p-3 bg-rose-950/60 border border-rose-800 text-rose-300 text-xs rounded-lg flex items-center gap-2">
                    <AlertCircle className="w-4 h-4 shrink-0" />
                    <span>{gameMessage}</span>
                  </div>
                )}

                {currentGame.state === 'WAITING' && (
                  <div className="mb-4 p-4 bg-amber-950/30 border border-amber-800/80 rounded-xl text-center">
                    <p className="text-sm font-medium text-amber-300">
                      Waiting for Opponent to Join
                    </p>
                    <p className="text-xs text-amber-400/80 mt-1">
                      Share room code <span className="font-mono font-bold text-white bg-slate-900 px-2 py-0.5 rounded">{currentGame.sessionId}</span> with another player on any device.
                    </p>
                  </div>
                )}

                {/* Match Concluded & Rematch Section */}
                {currentGame.state === 'FINISHED' && (
                  <div className="mb-4 p-4 bg-slate-900 border border-slate-700 rounded-xl text-center">
                    <Award className="w-8 h-8 text-amber-400 mx-auto mb-1.5" />
                    <h3 className="text-base font-bold text-white">
                      {currentGame.winner === 'DRAW'
                        ? 'Match Drawn (Stalemate)'
                        : `${currentGame.winner} Wins!`}
                    </h3>
                    <p className="text-xs text-slate-400 mt-0.5 mb-3">
                      Validated by authoritative game state machine
                    </p>

                    {/* Mutual Rematch Agreement Logic */}
                    {currentGame.isBot ? (
                      <button
                        onClick={handleRematch}
                        className="px-4 py-2 bg-cyan-500 hover:bg-cyan-400 text-slate-950 font-semibold text-xs rounded-lg transition-colors inline-flex items-center gap-1.5"
                      >
                        <RefreshCw className="w-3.5 h-3.5" />
                        Play Rematch vs Bot
                      </button>
                    ) : (
                      <div className="space-y-2">
                        {currentGame.rematchRequestedBy?.includes(user!) ? (
                          <div className="text-xs text-cyan-300 font-medium py-1">
                            ✓ You requested a rematch. Waiting for opponent to accept...
                          </div>
                        ) : (
                          <button
                            onClick={handleRematch}
                            className="px-4 py-2 bg-cyan-500 hover:bg-cyan-400 text-slate-950 font-semibold text-xs rounded-lg transition-colors inline-flex items-center gap-1.5"
                          >
                            <RefreshCw className="w-3.5 h-3.5" />
                            {currentGame.rematchRequestedBy && currentGame.rematchRequestedBy.length > 0
                              ? 'Opponent Wants a Rematch! (Click to Accept)'
                              : 'Request Rematch'}
                          </button>
                        )}
                      </div>
                    )}
                  </div>
                )}

                {currentGame.state === 'ABANDONED' && (
                  <div className="mb-4 p-4 bg-slate-900 border border-rose-800 rounded-xl text-center">
                    <p className="text-sm font-bold text-rose-300">Match Abandoned</p>
                    <p className="text-xs text-slate-400 mt-1 mb-3">
                      Winner: {currentGame.winner}
                    </p>
                    <button
                      onClick={handleLeaveRoom}
                      className="px-4 py-1.5 bg-slate-800 hover:bg-slate-700 text-white text-xs rounded"
                    >
                      Return to Lobby
                    </button>
                  </div>
                )}

                {/* 3x3 Grid */}
                <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-2xl flex flex-col items-center">
                  <div className="grid grid-cols-3 gap-3 w-72 h-72 sm:w-84 sm:h-84">
                    {[0, 1, 2].map((r) =>
                      [0, 1, 2].map((c) => {
                        const idx = r * 3 + c;
                        const marker = currentGame.board[idx] || ' ';
                        const isPlayerX = user === currentGame.playerX;
                        const isMyTurn =
                          currentGame.state === 'ACTIVE' &&
                          ((isPlayerX && currentGame.currentTurn === 'X') ||
                            (!isPlayerX && currentGame.currentTurn === 'O'));

                        return (
                          <button
                            key={`${r}-${c}`}
                            type="button"
                            onClick={() => handleCellClick(r, c)}
                            disabled={marker !== ' ' || !isMyTurn}
                            className={`rounded-xl flex items-center justify-center font-mono font-bold text-4xl sm:text-5xl transition-all duration-150 ${
                              marker === ' '
                                ? isMyTurn
                                  ? 'bg-slate-950 border border-slate-700 hover:border-cyan-400 hover:bg-slate-800/80 cursor-pointer'
                                  : 'bg-slate-950/60 border border-slate-800/50 cursor-not-allowed'
                                : marker === 'X'
                                ? 'bg-cyan-950/60 border border-cyan-700/80 text-cyan-400 shadow-md'
                                : 'bg-rose-950/60 border border-rose-700/80 text-rose-400 shadow-md'
                            }`}
                          >
                            {marker !== ' ' ? marker : ''}
                          </button>
                        );
                      })
                    )}
                  </div>

                  <div className="mt-4 text-xs text-slate-500 font-mono">
                    Authoritative 3x3 Grid · WebSockets Synchronized
                  </div>
                </div>
              </div>
            )}
          </div>
        )}

        {/* VIEW 2: OOP & C++ CODE INSPECTOR */}
        {activeTab === 'code' && (
          <div className="flex-1 flex flex-col space-y-6">
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-5 gap-4">
              <div className="bg-slate-900 border border-slate-800 rounded-xl p-4">
                <div className="flex items-center gap-2 text-cyan-400 mb-1.5 font-semibold text-sm">
                  <Shield className="w-4 h-4" />
                  <span>1. Encapsulation</span>
                </div>
                <p className="text-xs text-slate-400 leading-relaxed">
                  <strong className="text-slate-200">Board.h:</strong> <code className="text-cyan-300">grid[3][3]</code> is private. All mutations pass <code className="text-cyan-300">makeMove()</code> enforcing coordinate bounds and cell vacancy.
                </p>
              </div>

              <div className="bg-slate-900 border border-slate-800 rounded-xl p-4">
                <div className="flex items-center gap-2 text-indigo-400 mb-1.5 font-semibold text-sm">
                  <Layers className="w-4 h-4" />
                  <span>2. Abstraction</span>
                </div>
                <p className="text-xs text-slate-400 leading-relaxed">
                  <strong className="text-slate-200">ServerNetworkManager:</strong> Conceals networking routes, thread pool dispatching, and WebSocket frames behind clean public verbs.
                </p>
              </div>

              <div className="bg-slate-900 border border-slate-800 rounded-xl p-4">
                <div className="flex items-center gap-2 text-amber-400 mb-1.5 font-semibold text-sm">
                  <Cpu className="w-4 h-4" />
                  <span>3. Inheritance</span>
                </div>
                <p className="text-xs text-slate-400 leading-relaxed">
                  <strong className="text-slate-200">Player Hierarchy:</strong> <code className="text-amber-300">WebSocketPlayer</code>, <code className="text-amber-300">LocalPlayer</code>, and <code className="text-amber-300">AIPlayer</code> derive from abstract <code className="text-amber-300">Player</code>.
                </p>
              </div>

              <div className="bg-slate-900 border border-slate-800 rounded-xl p-4">
                <div className="flex items-center gap-2 text-emerald-400 mb-1.5 font-semibold text-sm">
                  <Terminal className="w-4 h-4" />
                  <span>4. Polymorphism</span>
                </div>
                <p className="text-xs text-slate-400 leading-relaxed">
                  <strong className="text-slate-200">Dynamic Dispatch:</strong> <code className="text-emerald-300">virtual getMove() = 0</code> allows GameSession to orchestrate games through base pointers with a virtual destructor.
                </p>
              </div>

              <div className="bg-slate-900 border border-emerald-800/60 rounded-xl p-4">
                <div className="flex items-center gap-2 text-emerald-400 mb-1.5 font-semibold text-sm">
                  <Cpu className="w-4 h-4" />
                  <span>5. DSA Minimax Bot</span>
                </div>
                <p className="text-xs text-slate-400 leading-relaxed">
                  <strong className="text-slate-200">AIPlayer.cpp:</strong> Zero-sum game tree search (O(9!) leaves). Base cases: AI Win (+10 - d), Loss (d - 10), Draw (0). Backtracks using <code className="text-emerald-300">undoMove()</code>.
                </p>
              </div>
            </div>

            {/* Code Explorer */}
            <div className="bg-slate-900 border border-slate-800 rounded-xl flex-1 flex flex-col overflow-hidden min-h-[500px]">
              <div className="p-3 border-b border-slate-800 bg-slate-950 flex items-center gap-2 overflow-x-auto">
                <span className="text-xs text-slate-500 font-mono px-2 shrink-0">SOURCE FILES:</span>
                {cppFiles.map((file) => (
                  <button
                    key={file.path}
                    onClick={() => setSelectedFilePath(file.path)}
                    className={`px-2.5 py-1 text-xs font-mono rounded transition-colors whitespace-nowrap shrink-0 ${
                      selectedFilePath === file.path
                        ? 'bg-cyan-500 text-slate-950 font-semibold'
                        : 'bg-slate-900 text-slate-400 hover:text-white border border-slate-800'
                    }`}
                  >
                    {file.path}
                  </button>
                ))}
              </div>

              <div className="flex-1 p-4 bg-slate-950 overflow-auto font-mono text-xs text-slate-300">
                {loadingFiles ? (
                  <div className="h-full flex items-center justify-center text-slate-500">
                    <RefreshCw className="w-5 h-5 animate-spin mr-2" />
                    <span>Loading source files...</span>
                  </div>
                ) : selectedFile ? (
                  <div className="relative">
                    <div className="flex items-center justify-between pb-3 mb-3 border-b border-slate-800 text-[11px] text-slate-400">
                      <span>File: {selectedFile.path}</span>
                      <button
                        onClick={() => {
                          navigator.clipboard.writeText(selectedFile.content);
                          setCopiedCode(true);
                          setTimeout(() => setCopiedCode(false), 2000);
                        }}
                        className="px-2 py-1 bg-slate-800 hover:bg-slate-700 text-slate-300 rounded flex items-center gap-1 transition-colors"
                      >
                        {copiedCode ? <Check className="w-3 h-3 text-emerald-400" /> : <Copy className="w-3 h-3" />}
                        <span>Copy File</span>
                      </button>
                    </div>
                    <pre className="overflow-x-auto leading-relaxed">
                      {selectedFile.content}
                    </pre>
                  </div>
                ) : (
                  <div className="text-slate-500">Select a file to inspect.</div>
                )}
              </div>
            </div>
          </div>
        )}
      </main>
    </div>
  );
}
