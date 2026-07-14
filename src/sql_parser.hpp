#pragma once

#include <string>
#include <vector>
#include "database.hpp"

enum class SqlType {
    SELECT_KEY,
    SELECT_ALL,
    INSERT,
    UPDATE,
    DELETE_KEY,
    INVALID
};

struct SqlCommand {
    SqlType type = SqlType::INVALID;
    std::string key;
    std::string value;
    std::string errorMessage;
};

class SqlParser {
public:
    // Parse an SQL statement string and return an SQLCommand structure
    static SqlCommand parse(const std::string& sqlQuery);

    // Executes the SQL query against a database instance and returns a formatted result string
    static std::string execute(Database& db, const std::string& sqlQuery, bool isFollower);

private:
    static std::string trim(const std::string& str);
    static std::string toUpper(const std::string& str);
    static bool startsWith(const std::string& str, const std::string& prefix);
    static std::string formatTable(const std::vector<std::pair<std::string, std::string>>& rows);
};
