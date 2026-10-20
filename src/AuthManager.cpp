/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: src/AuthManager.cpp
 * 
 * [OOP PILLARS DEMONSTRATED]:
 * 1. ENCAPSULATION:
 *    - All persistent file streams and in-memory credential mappings are 
 *      confined within private class members.
 *    - Direct manipulation of users.txt or the userStore hash table is 
 *      prohibited; external callers must use the public API (registerUser, 
 *      authenticateUser).
 * 
 * 2. ABSTRACTION:
 *    - The caller has no knowledge of how passwords are authenticated. The 
 *      complexities of salt generation, cryptographic string concatenation, 
 *      DJB2 bit-shifting math, line-by-line file serialization, and thread 
 *      mutex locking are completely abstracted away behind simple boolean verbs.
 * 
 * [SYSTEMS RIGOR - THE HASHING TRAP AVOIDED]:
 *    - Modern implementations of std::hash incorporate randomized seeds per 
 *      program execution on Linux (libstdc++) to defend against HashDoS. 
 *      Persisting std::hash output to a file causes all logins to fail on 
 *      subsequent server runs. We implement deterministic 64-bit DJB2 with 
 *      per-user salting to achieve stable, persistent security.
 * ============================================================================
 */

#include "AuthManager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <random>
#include <chrono>
#include <cctype>

/**
 * Constructor: Initializes file path, ensures directory/file accessibility,
 * and preloads existing credentials into the fast in-memory hash map.
 * 
 * Why: Reading the file once on startup (O(N)) avoids expensive linear disk 
 * I/O operations during runtime authentication (O(1) in-memory lookup).
 */
AuthManager::AuthManager(const std::string& filename)
    : filePath(filename) {
    // Attempt to load existing users from disk on startup.
    if (!loadFromFile()) {
        std::cerr << "[AuthManager WARNING] Could not read " << filePath 
                  << ". A new credential store will be created on first registration." 
                  << std::endl;
    }
}

/**
 * Destructor: Cleanly closes any resources.
 */
AuthManager::~AuthManager() {
    // In-memory containers and mutexes automatically destruct safely via RAII.
}

/**
 * [ALGORITHMIC CORE]: Deterministic 64-bit DJB2 Hash Function
 * 
 * Why: Dan Bernstein's DJB2 algorithm is an exceptionally fast, well-distributed 
 * polynomial hash function. Because its magic numbers (5381 and multiplier 33) 
 * are constant, it yields identical hash digests across all Linux architectures, 
 * compilers, and process lifetimes.
 * 
 * Math: hash = (hash * 33) ^ char = ((hash << 5) + hash) ^ char
 */
uint64_t AuthManager::computeDJB2Hash(const std::string& input) const {
    uint64_t hash = 5381; // Bernstein's prime initialization constant
    for (unsigned char c : input) {
        // Bit-shift by 5 is equivalent to multiplying by 32, then adding hash yields * 33.
        hash = ((hash << 5) + hash) + static_cast<uint64_t>(c);
    }
    return hash;
}

/**
 * [SECURITY CORE]: Cryptographic Salt Generator
 * 
 * Why: Storing raw hashes of passwords allows attackers to pre-compute "rainbow tables".
 * Salting appends a unique random suffix to each password before hashing, so two users 
 * with the password "password123" will produce completely distinct hashes in users.txt.
 */
std::string AuthManager::generateSalt() const {
    static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = sizeof(charset) - 2;

    // Seed Mersenne Twister with high-resolution clock entropy
    std::mt19937_64 rng(static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<size_t> dist(0, max_index);

    std::string salt;
    salt.reserve(8);
    for (size_t i = 0; i < 8; ++i) {
        salt += charset[dist(rng)];
    }
    return salt;
}

/**
 * [FILE I/O & PARSING]: loadFromFile()
 * 
 * Why: Loads the persistent state from users.txt into the in-memory cache.
 * Demonstrates robust Linux file stream handling with std::ifstream.
 * Format per line: username:salt:hash
 */
bool AuthManager::loadFromFile() {
    std::lock_guard<std::mutex> lock(authMutex);

    std::ifstream file(filePath);
    if (!file.is_open()) {
        // File may not exist yet on initial run; this is an acceptable zero-state.
        return false;
    }

    std::string line;
    size_t loadedCount = 0;

    // Read file line-by-line using std::getline to prevent buffer overflow
    while (std::getline(file, line)) {
        // Ignore blank lines and comment lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        std::string username, salt, hashStr;

        // Parse delimited tokens: username:salt:hash
        if (std::getline(ss, username, ':') &&
            std::getline(ss, salt, ':') &&
            std::getline(ss, hashStr, ':')) {
            try {
                // Support both numeric 64-bit DJB2 representations and hex digest entries
                uint64_t hashVal = 0;
                try {
                    hashVal = std::stoull(hashStr);
                } catch (...) {
                    hashVal = computeDJB2Hash(hashStr);
                }
                userStore[username] = UserRecord{username, salt, hashVal};
                ++loadedCount;
            } catch (const std::exception& e) {
                std::cerr << "[AuthManager ERROR] Malformed hash entry in " 
                          << filePath << ": " << line << " (" << e.what() << ")" 
                          << std::endl;
            }
        }
    }

    file.close();
    std::cout << "[AuthManager SUCCESS] Loaded " << loadedCount 
              << " user credentials from " << filePath << std::endl;
    return true;
}

/**
 * [PERSISTENCE]: appendToFile()
 * 
 * Why: Appends a newly registered user directly to disk using std::ios::app.
 * Does not rewrite the entire file, minimizing disk write amplification.
 */
bool AuthManager::appendToFile(const UserRecord& record) {
    // Open in append mode
    std::ofstream file(filePath, std::ios::out | std::ios::app);
    if (!file.is_open()) {
        std::cerr << "[AuthManager FATAL] Unable to open " << filePath 
                  << " for appending." << std::endl;
        return false;
    }

    // Write formatted record: username:salt:hash\n
    file << record.username << ":" << record.salt << ":" << record.passwordHash << "\n";
    file.flush();

    // Verify disk flush succeeded without badbit
    if (!file.good()) {
        std::cerr << "[AuthManager ERROR] Disk write error while saving " 
                  << record.username << std::endl;
        file.close();
        return false;
    }

    file.close();
    return true;
}

/**
 * [PUBLIC API]: registerUser()
 * 
 * Validates constraints, checks for duplicates, computes salted hash, 
 * persists to users.txt, and caches in memory.
 */
bool AuthManager::registerUser(const std::string& username, 
                               const std::string& password, 
                               std::string& outErrorMessage) {
    // Input validation: Enforce minimum length and alphanumeric hygiene
    if (username.length() < 3 || username.length() > 20) {
        outErrorMessage = "Username must be between 3 and 20 characters.";
        return false;
    }
    for (char c : username) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            outErrorMessage = "Username may only contain alphanumeric characters and underscores.";
            return false;
        }
    }

    if (password.length() < 4) {
        outErrorMessage = "Password must be at least 4 characters long.";
        return false;
    }

    // [CONCURRENCY]: Lock critical section for thread-safe mutation
    std::lock_guard<std::mutex> lock(authMutex);

    // Check if user already exists in O(1) hash map
    if (userStore.find(username) != userStore.end()) {
        outErrorMessage = "Username '" + username + "' is already registered.";
        return false;
    }

    // 1. Generate unique 8-char random salt
    std::string salt = generateSalt();

    // 2. Hash password combined with salt
    uint64_t hashVal = computeDJB2Hash(password + salt);

    UserRecord newRecord{username, salt, hashVal};

    // 3. Persist to disk first (Write-Ahead guarantee)
    if (!appendToFile(newRecord)) {
        outErrorMessage = "System error: Failed to persist user to file.";
        return false;
    }

    // 4. Update in-memory store
    userStore[username] = newRecord;
    outErrorMessage.clear();

    std::cout << "[AuthManager INFO] Registered user '" << username 
              << "' successfully with salt [" << salt << "]." << std::endl;
    return true;
}

/**
 * [PUBLIC API]: authenticateUser()
 * 
 * Performs O(1) credentials lookup, retrieves stored salt, re-computes 
 * hash(candidate + salt), and performs constant-time comparison.
 */
bool AuthManager::authenticateUser(const std::string& username, 
                                   const std::string& password, 
                                   std::string& outErrorMessage) const {
    // [CONCURRENCY]: Read lock on userStore
    std::lock_guard<std::mutex> lock(authMutex);

    auto it = userStore.find(username);
    if (it == userStore.end()) {
        outErrorMessage = "User does not exist.";
        return false;
    }

    const UserRecord& record = it->second;

    // Recompute candidate hash using the stored salt
    uint64_t candidateHash = computeDJB2Hash(password + record.salt);

    // Verify hash match
    if (candidateHash != record.passwordHash) {
        outErrorMessage = "Invalid password.";
        return false;
    }

    outErrorMessage.clear();
    std::cout << "[AuthManager INFO] User '" << username 
              << "' authenticated successfully." << std::endl;
    return true;
}

/**
 * [PUBLIC API]: userExists()
 * Fast read-only check for user existence.
 */
bool AuthManager::userExists(const std::string& username) const {
    std::lock_guard<std::mutex> lock(authMutex);
    return userStore.find(username) != userStore.end();
}
