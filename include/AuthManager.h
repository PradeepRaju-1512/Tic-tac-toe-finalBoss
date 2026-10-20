/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: include/AuthManager.h
 * 
 * [OOP PILLAR APPLIED: ENCAPSULATION & ABSTRACTION]
 * 1. Encapsulation: The internal representation of the credentials database
 *    (file stream handles, salt generation, and in-memory hash mappings)
 *    is strictly private. External classes cannot modify the user store
 *    without going through validated public member functions.
 * 2. Abstraction: The caller only interacts with high-level concepts
 *    like registerUser() and authenticateUser(). The underlying file format,
 *    disk flushing, and mathematical DJB2 hashing mechanics are completely
 *    hidden from the caller.
 * 
 * [CRITICAL SYSTEMS NOTE: THE HASHING TRAP AVOIDED]
 * We deliberately implement the deterministic 64-bit DJB2 algorithm rather than
 * std::hash. std::hash is randomized per process invocation on modern GCC/Clang
 * libstdc++ runtimes to protect against HashDoS attacks. If std::hash were used,
 * persisted password hashes in users.txt would be rendered permanently invalid
 * upon server restart.
 * ============================================================================
 */

#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <string>
#include <unordered_map>
#include <cstdint>
#include <mutex>

/**
 * @struct UserRecord
 * @brief Plain Old Data (POD) structure representing a persisted user record.
 * 
 * Stores the unique username, a random per-user salt string (to protect against
 * rainbow table attacks), and the computed 64-bit deterministic hash.
 */
struct UserRecord {
    std::string username;
    std::string salt;
    uint64_t passwordHash;
};

/**
 * @class AuthManager
 * @brief Thread-safe file-backed user authentication and credential manager.
 */
class AuthManager {
private:
    // [ENCAPSULATION]: File path to users.txt is strictly private.
    std::string filePath;

    // [ENCAPSULATION]: In-memory cache of username -> UserRecord.
    // Private to prevent external tampering without file sync.
    std::unordered_map<std::string, UserRecord> userStore;

    // Mutex for thread safety across concurrent Crow HTTP/WebSocket worker threads.
    mutable std::mutex authMutex;

    /**
     * @brief Computes a deterministic 64-bit DJB2 hash of the input string.
     * 
     * DJB2 is Dan Bernstein's classic algorithm (hash * 33 + c). It is 100%
     * reproducible across server restarts and different Linux machines, unlike
     * std::hash which varies across process runs.
     * 
     * @param input The salted password string.
     * @return uint64_t Deterministic 64-bit hash digest.
     */
    uint64_t computeDJB2Hash(const std::string& input) const;

    /**
     * @brief Generates an 8-character pseudo-random cryptographic salt.
     * 
     * Salting ensures identical passwords generate completely different stored
     * hashes, preventing dictionary and rainbow table attacks.
     * 
     * @return std::string The generated salt string.
     */
    std::string generateSalt() const;

    /**
     * @brief Loads all user credentials from users.txt into the memory store.
     * 
     * Called during class construction. Opens users.txt via std::ifstream,
     * parses line by line (format: username:salt:hash), and populates userStore.
     * 
     * @return true if file read successfully or file newly created, false on I/O error.
     */
    bool loadFromFile();

    /**
     * @brief Appends a newly registered user directly to users.txt.
     * 
     * Uses std::ofstream in append mode (std::ios::app) to ensure persistence.
     * 
     * @param record The new user record to persist.
     * @return true if appended successfully, false on disk error.
     */
    bool appendToFile(const UserRecord& record);

public:
    /**
     * @brief Constructor for AuthManager.
     * @param filename Path to users.txt (defaults to "users.txt").
     */
    explicit AuthManager(const std::string& filename = "users.txt");

    /**
     * @brief Destructor. Ensures all pending file writes are finalized.
     */
    ~AuthManager();

    // Prevent copy semantics to avoid race conditions on file handles (Single Source of Truth).
    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;

    /**
     * @brief Registers a new user with username and plaintext password.
     * 
     * Generates a unique salt, hashes (password + salt) using computeDJB2Hash(),
     * persists to users.txt, and caches in memory.
     * 
     * @param username Desired alphanumeric username.
     * @param password Plaintext password submitted by user.
     * @param outErrorMessage Detailed error message if registration fails.
     * @return true if registration succeeded, false if user already exists or invalid input.
     */
    bool registerUser(const std::string& username, const std::string& password, std::string& outErrorMessage);

    /**
     * @brief Authenticates an existing user against stored salt and hash.
     * 
     * Looks up username in memory, retrieves stored salt, recomputes
     * computeDJB2Hash(password + salt), and compares in constant time.
     * 
     * @param username Registered username.
     * @param password Candidate plaintext password.
     * @param outErrorMessage Detailed reason on failure.
     * @return true if credentials match, false otherwise.
     */
    bool authenticateUser(const std::string& username, const std::string& password, std::string& outErrorMessage) const;

    /**
     * @brief Checks if a username is already taken.
     * @param username The username to check.
     * @return true if user exists, false otherwise.
     */
    bool userExists(const std::string& username) const;
};

#endif // AUTH_MANAGER_H
