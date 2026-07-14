#pragma once

#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include "network.hpp"
#include "database.hpp"

class ReplicationManager {
public:
    ReplicationManager(Database& db, bool isLeader);
    ~ReplicationManager();

    // Leader functions
    void registerFollower(SOCKET sock);
    void broadcastWrite(const std::string& op, const std::string& key, const std::string& value = "");

    // Follower functions
    void startFollowerSync(const std::string& leaderIp, int leaderPort, const std::string& nodeId);
    void stopFollowerSync();
    bool isSynced() const { return m_synced; }

private:
    Database& m_db;
    bool m_isLeader;
    
    // Leader state
    std::vector<SOCKET> m_followers;
    mutable std::mutex m_followersMutex;

    // Follower state
    std::thread m_followerThread;
    std::atomic<bool> m_runFollower;
    std::atomic<bool> m_synced;
    SOCKET m_leaderSocket;

    void followerLoop(std::string leaderIp, int leaderPort, std::string nodeId);
};
