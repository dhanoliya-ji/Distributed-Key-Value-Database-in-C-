#include "sql_parser.hpp"
#include <sstream>
#include <algorithm>
#include <iomanip>

std::string SqlParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string SqlParser::toUpper(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return upper;
}

bool SqlParser::startsWith(const std::string& str, const std::string& prefix) {
    if (str.length() < prefix.length()) return false;
    return str.compare(0, prefix.length(), prefix) == 0;
}

static std::vector<std::string> extractQuotedStrings(const std::string& str) {
    std::vector<std::string> result;
    bool inQuote = false;
    char quoteChar = 0;
    std::string current;
    for (size_t i = 0; i < str.length(); ++i) {
        char c = str[i];
        if (c == '\'' || c == '"') {
            if (inQuote && c == quoteChar) {
                result.push_back(current);
                current.clear();
                inQuote = false;
            } else if (!inQuote) {
                inQuote = true;
                quoteChar = c;
            } else {
                current += c;
            }
        } else if (inQuote) {
            if (c == '\\' && i + 1 < str.length()) {
                current += str[i+1];
                i++;
            } else {
                current += c;
            }
        }
    }
    return result;
}

SqlCommand SqlParser::parse(const std::string& sqlQuery) {
    SqlCommand cmd;
    std::string trimmed = trim(sqlQuery);
    if (trimmed.empty()) {
        cmd.type = SqlType::INVALID;
        cmd.errorMessage = "Empty SQL query.";
        return cmd;
    }

    std::string upperQuery = toUpper(trimmed);

    // 1. SELECT Query
    if (startsWith(upperQuery, "SELECT")) {
        // Check for SELECT * FROM kv or SELECT key, value FROM kv
        if (upperQuery == "SELECT * FROM KV" || upperQuery == "SELECT KEY, VALUE FROM KV" || 
            upperQuery == "SELECT * FROM KV;" || upperQuery == "SELECT KEY, VALUE FROM KV;") {
            cmd.type = SqlType::SELECT_ALL;
            return cmd;
        }

        // Must be SELECT WHERE key = '...'
        size_t wherePos = upperQuery.find("WHERE");
        if (wherePos == std::string::npos) {
            cmd.type = SqlType::INVALID;
            cmd.errorMessage = "Syntax Error: SELECT query missing WHERE clause or SELECT ALL syntax incorrect.";
            return cmd;
        }

        std::string whereClause = trimmed.substr(wherePos);
        std::string upperWhere = toUpper(whereClause);
        size_t keyPos = upperWhere.find("KEY");
        size_t eqPos = upperWhere.find("=");
        if (keyPos == std::string::npos || eqPos == std::string::npos || eqPos < keyPos) {
            cmd.type = SqlType::INVALID;
            cmd.errorMessage = "Syntax Error: WHERE clause must specify 'key = ...'";
            return cmd;
        }

        auto quotes = extractQuotedStrings(whereClause);
        if (quotes.empty()) {
            cmd.type = SqlType::INVALID;
            cmd.errorMessage = "Syntax Error: Target key must be enclosed in single/double quotes.";
            return cmd;
        }

        cmd.type = SqlType::SELECT_KEY;
        cmd.key = quotes[0];
        return cmd;
    }

    // 2. INSERT Query
    if (startsWith(upperQuery, "INSERT")) {
        // Expecting: INSERT INTO kv (key, value) VALUES ('k', 'v') or INSERT INTO kv VALUES ('k', 'v')
        size_t valuesPos = upperQuery.find("VALUES");
        if (valuesPos == std::string::npos) {
            cmd.type = SqlType::INVALID;
            cmd.errorMessage = "Syntax Error: INSERT query missing VALUES clause.";
            return cmd;
        }

        std::string valuesClause = trimmed.substr(valuesPos);
        auto quotes = extractQuotedStrings(valuesClause);
        if (quotes.size() < 2) {
            cmd.type = SqlType::INVALID;
            cmd.errorMessage = "Syntax Error: INSERT VALUES must provide both key and value in quotes.";
            return cmd;
        }

        cmd.type = SqlType::INSERT;
        cmd.key = quotes[0];
        cmd.value = quotes[1];
        return cmd;
    }

    // 3. UPDATE Query
    if (startsWith(upperQuery, "UPDATE")) {
        // Expecting: UPDATE kv SET value = 'v' WHERE key = 'k'
        size_t setPos = upperQuery.find("SET");
        size_t wherePos = upperQuery.find("WHERE");
        if (setPos == std::string::npos || wherePos == std::string::npos || wherePos < setPos) {
            cmd.type = SqlType::INVALID;
            cmd.errorMessage = "Syntax Error: UPDATE query must contain SET and WHERE clauses in order.";
            return cmd;
        }

        std::string setPart = trimmed.substr(setPos, wherePos - setPos);
        std::string wherePart = trimmed.substr(wherePos);

        auto setQuotes = extractQuotedStrings(setPart);
        auto whereQuotes = extractQuotedStrings(wherePart);

        if (setQuotes.empty() || whereQuotes.empty()) {
            cmd.type = SqlType::INVALID;
            cmd.errorMessage = "Syntax Error: UPDATE requires quoted value in SET and quoted key in WHERE.";
            return cmd;
        }

        cmd.type = SqlType::UPDATE;
        cmd.key = whereQuotes[0];
        cmd.value = setQuotes[0];
        return cmd;
    }

    // 4. DELETE Query
    if (startsWith(upperQuery, "DELETE")) {
        // Expecting: DELETE FROM kv WHERE key = 'k'
        size_t wherePos = upperQuery.find("WHERE");
        if (wherePos == std::string::npos) {
            cmd.type = SqlType::INVALID;
            cmd.errorMessage = "Syntax Error: DELETE query missing WHERE clause.";
            return cmd;
        }

        std::string wherePart = trimmed.substr(wherePos);
        auto quotes = extractQuotedStrings(wherePart);
        if (quotes.empty()) {
            cmd.type = SqlType::INVALID;
            cmd.errorMessage = "Syntax Error: DELETE requires target key in WHERE clause.";
            return cmd;
        }

        cmd.type = SqlType::DELETE_KEY;
        cmd.key = quotes[0];
        return cmd;
    }

    cmd.type = SqlType::INVALID;
    cmd.errorMessage = "Syntax Error: Unsupported SQL statement. Supported statements: SELECT, INSERT, UPDATE, DELETE.";
    return cmd;
}

std::string SqlParser::formatTable(const std::vector<std::pair<std::string, std::string>>& rows) {
    if (rows.empty()) {
        return "Empty set.\n";
    }

    size_t maxKeyLen = 3;   // minimum header "key" length
    size_t maxValLen = 5;   // minimum header "value" length

    for (const auto& row : rows) {
        maxKeyLen = std::max(maxKeyLen, row.first.length());
        maxValLen = std::max(maxValLen, row.second.length());
    }

    std::stringstream ss;
    
    // Helper to write horizontal separator
    auto writeSeparator = [&]() {
        ss << "+" << std::string(maxKeyLen + 2, '-') << "+" << std::string(maxValLen + 2, '-') << "+\n";
    };

    writeSeparator();
    // Headers
    ss << "| " << std::left << std::setw(maxKeyLen) << "key" << " | " << std::setw(maxValLen) << "value" << " |\n";
    writeSeparator();
    
    // Rows
    for (const auto& row : rows) {
        ss << "| " << std::left << std::setw(maxKeyLen) << row.first << " | " << std::setw(maxValLen) << row.second << " |\n";
    }
    writeSeparator();
    
    ss << rows.size() << " row(s) in set.\n";
    return ss.str();
}

std::string SqlParser::execute(Database& db, const std::string& sqlQuery, bool isFollower) {
    SqlCommand cmd = parse(sqlQuery);
    if (cmd.type == SqlType::INVALID) {
        return "ERROR: " + cmd.errorMessage + "\n";
    }

    // Follower validation: reject write commands
    if (isFollower && (cmd.type == SqlType::INSERT || cmd.type == SqlType::UPDATE || cmd.type == SqlType::DELETE_KEY)) {
        return "ERROR: Node is a Follower. Write operations are read-only.\n";
    }

    switch (cmd.type) {
        case SqlType::SELECT_KEY: {
            std::string value;
            if (db.get(cmd.key, value)) {
                std::vector<std::pair<std::string, std::string>> result = {{cmd.key, value}};
                return formatTable(result);
            } else {
                return "Empty set (key not found).\n";
            }
        }
        case SqlType::SELECT_ALL: {
            return formatTable(db.getAllEntries());
        }
        case SqlType::INSERT: {
            std::string temp;
            if (db.get(cmd.key, temp)) {
                return "ERROR: Key already exists. Use UPDATE to modify.\n";
            }
            db.put(cmd.key, cmd.value);
            return "Query OK, 1 row affected (INSERT).\n";
        }
        case SqlType::UPDATE: {
            std::string temp;
            if (!db.get(cmd.key, temp)) {
                return "ERROR: Key does not exist. Use INSERT to create.\n";
            }
            db.put(cmd.key, cmd.value);
            return "Query OK, 1 row affected (UPDATE).\n";
        }
        case SqlType::DELETE_KEY: {
            if (db.del(cmd.key)) {
                return "Query OK, 1 row affected (DELETE).\n";
            } else {
                return "Query OK, 0 rows affected (key not found).\n";
            }
        }
        default:
            return "ERROR: Unimplemented command.\n";
    }
}
