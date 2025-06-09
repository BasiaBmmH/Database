#include "database.h"
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <fmt/core.h>

DataType Database::parseType(const std::string& typeStr) {
    if (typeStr == "INT") return DataType::INT;
    if (typeStr == "STRING") return DataType::STRING;
    throw std::runtime_error("Unknown type: " + typeStr);
}

auto Database::getColumnIndex(const Table& table, const std::string& columnName) -> int {
    for (size_t i = 0; i < table.columns.size(); ++i) {
        if (table.columns[i].name == columnName)
            return i;
    }
    throw std::runtime_error("Column not found: " + columnName);
}

auto Database::evaluateCondition(const Table& table, const Row& row,
                                 const std::string& colName, const std::string& op, const std::string& valueStr) -> bool {
    int index = getColumnIndex(table, colName);
    const auto& val = row[index];

    if (table.columns[index].type == DataType::INT) {
        int cmpVal = std::stoi(valueStr);
        int rowVal = std::get<int>(val);
        if (op == "=") return rowVal == cmpVal;
        if (op == "!=") return rowVal != cmpVal;
        if (op == "<") return rowVal < cmpVal;
        if (op == ">") return rowVal > cmpVal;
    } else {
        std::string cmpVal = valueStr;
        if (cmpVal.front() == '"' && cmpVal.back() == '"')
            cmpVal = cmpVal.substr(1, cmpVal.size() - 2);
        std::string rowVal = std::get<std::string>(val);
        if (op == "=") return rowVal == cmpVal;
        if (op == "!=") return rowVal != cmpVal;
    }
    return false;
}

void Database::execute(const std::string& query) {
    std::smatch m;

    // CREATE TABLE
    if (std::regex_match(query, m, std::regex("CREATE TABLE (\\w+)\\s*\\((.+)\\)"))) {
        std::string tableName = m[1];
        std::string cols = m[2];

        Table table;
        table.name = tableName;
        std::regex colRegex("(\\w+)\\s+(INT|STRING)");
        auto colBegin = std::sregex_iterator(cols.begin(), cols.end(), colRegex);
        for (auto it = colBegin; it != std::sregex_iterator(); ++it) {
            table.columns.push_back({(*it)[1], parseType((*it)[2])});
        }
        tables[tableName] = table;
        fmt::print("Table {} created.\n", tableName);
    }

    // INSERT INTO
    else if (std::regex_match(query, m, std::regex("INSERT INTO (\\w+) VALUES\\s*\\((.+)\\)"))) {
        std::string tableName = m[1];
        std::string values = m[2];
        auto& table = tables.at(tableName);

        Row row;
        std::regex valRegex("\"([^\"]*)\"|(\\d+)");
        auto valBegin = std::sregex_iterator(values.begin(), values.end(), valRegex);
        for (auto it = valBegin; it != std::sregex_iterator(); ++it) {
            if ((*it)[2].matched)
                row.emplace_back(std::stoi((*it)[2]));
            else
                row.emplace_back((*it)[1].str());
        }
        if (row.size() != table.columns.size()) {
            throw std::runtime_error("Column count mismatch.");
        }
        table.rows.push_back(row);
        fmt::print("Row inserted into {}.\n", tableName);
    }

    // SELECT * FROM ... WHERE ...
    // SELECT col1, col2 FROM table [WHERE col op val]
    else if (std::regex_match(query, m, std::regex(R"(SELECT\s+(.+?)\s+FROM\s+(\w+)(?:\s+WHERE\s+(\w+)\s*([=!<>]+)\s*(.+))?)"))) {
        std::string colPart = m[1];
        std::string tableName = m[2];
        auto& table = tables.at(tableName);

        // Parse selected columns
        std::vector<int> selectedIndexes;
        if (colPart == "*") {
            for (size_t i = 0; i < table.columns.size(); ++i)
                selectedIndexes.push_back(i);
        } else {
            std::regex colRegex(R"(\w+)");
            auto colBegin = std::sregex_iterator(colPart.begin(), colPart.end(), colRegex);
            for (auto it = colBegin; it != std::sregex_iterator(); ++it) {
                selectedIndexes.push_back(getColumnIndex(table, (*it).str()));
            }
        }

        // Optional WHERE
        bool hasCond = m[3].matched;
        std::string condCol, condOp, condVal;
        if (hasCond) {
            condCol = m[3];
            condOp = m[4];
            condVal = m[5];
        }

        // Header
        for (int idx : selectedIndexes)
            fmt::print("{}\t", table.columns[idx].name);
        fmt::print("\n");

        // Rows
        for (const auto& row : table.rows) {
            if (!hasCond || evaluateCondition(table, row, condCol, condOp, condVal)) {
                for (int idx : selectedIndexes)
                    std::visit([](auto&& arg) { fmt::print("{}\t", arg); }, row[idx]);
                fmt::print("\n");
            }
        }
    }


        // DELETE FROM ... WHERE ...
    else if (std::regex_match(query, m, std::regex("DELETE FROM (\\w+)\\s+WHERE (\\w+)\\s*([=!<>]+)\\s*(.+)"))) {
        std::string tableName = m[1];
        std::string colName = m[2], op = m[3], val = m[4];
        auto& table = tables.at(tableName);
        auto& rows = table.rows;

        auto it = std::remove_if(rows.begin(), rows.end(),
                                 [&](const Row& row) { return evaluateCondition(table, row, colName, op, val); });
        int count = std::distance(it, rows.end());
        rows.erase(it, rows.end());
        fmt::print("{} row(s) deleted from {}\n", count, tableName);
    }

    // ADD COLUMN table col type
    else if (std::regex_match(query, m, std::regex("ADD COLUMN (\\w+)\\s+(\\w+)\\s+(INT|STRING)"))) {
        std::string tableName = m[1];
        std::string colName = m[2];
        DataType type = parseType(m[3]);
        auto& table = tables.at(tableName);

        table.columns.push_back({colName, type});
        for (auto& row : table.rows) {
            if (type == DataType::INT)
                row.push_back(0);
            else
                row.push_back(std::string(""));
        }
        fmt::print("Column {} added to table {}\n", colName, tableName);
    }

    // SAVE TO "file"
    else if (std::regex_match(query, m, std::regex("SAVE TO \"(.*)\""))) {
        std::ofstream out(m[1]);
        if (!out) throw std::runtime_error("Cannot open file for writing.");
        for (const auto& [name, table] : tables) {
            out << "CREATE TABLE " << name << " (";
            for (size_t i = 0; i < table.columns.size(); ++i) {
                out << table.columns[i].name << " "
                    << (table.columns[i].type == DataType::INT ? "INT" : "STRING");
                if (i + 1 < table.columns.size()) out << ", ";
            }
            out << ")\n";
            for (const auto& row : table.rows) {
                out << "INSERT INTO " << name << " VALUES (";
                for (size_t i = 0; i < row.size(); ++i) {
                    std::visit([&](auto&& arg) {
                        if constexpr (std::is_same_v<decltype(arg), std::string>)
                            out << "\"" << arg << "\"";
                        else
                            out << arg;
                    }, row[i]);
                    if (i + 1 < row.size()) out << ", ";
                }
                out << ")\n";
            }
        }
        fmt::print("Database saved to {}.\n", m[1].str());
    }
    // LOAD FROM "file"
    else if (std::regex_match(query, m, std::regex("LOAD FROM \"(.*)\""))) {
        std::ifstream in(m[1]);
        if (!in) throw std::runtime_error("Cannot open file for reading.");
        std::string line;
        while (getline(in, line)) {
            execute(line);
        }
        fmt::print("Database loaded from {}.\n", m[1].str());
    }

        // UPDATE table SET col1=val1, col2=val2 WHERE cond1 AND cond2 ...
    else if (std::regex_match(query, m, std::regex(R"(UPDATE (\w+)\s+SET\s+(.+)\s+WHERE\s+(.+))"))) {
        std::string tableName = m[1];
        std::string setPart = m[2];
        std::string wherePart = m[3];

        auto& table = tables.at(tableName);
        std::vector<std::pair<int, std::string>> updates;

        // Parse SET col=val, col2=val2 ...
        std::regex setRegex(R"((\w+)\s*=\s*("[^"]*"|\d+))");
        auto setBegin = std::sregex_iterator(setPart.begin(), setPart.end(), setRegex);
        for (auto it = setBegin; it != std::sregex_iterator(); ++it) {
            int index = getColumnIndex(table, (*it)[1]);
            updates.emplace_back(index, (*it)[2]);
        }

        // Parse WHERE cond AND cond ...
        std::vector<std::tuple<int, std::string, std::string>> conditions;
        std::regex condRegex(R"((\w+)\s*([=!<>]+)\s*("[^"]*"|\d+))");
        auto condBegin = std::sregex_iterator(wherePart.begin(), wherePart.end(), condRegex);
        for (auto it = condBegin; it != std::sregex_iterator(); ++it) {
            int index = getColumnIndex(table, (*it)[1]);
            conditions.emplace_back(index, (*it)[2], (*it)[3]);
        }

        int updatedCount = 0;

        for (auto& row : table.rows) {
            bool match = true;
            for (const auto& [colIdx, op, val] : conditions) {
                if (!evaluateCondition(table, row, table.columns[colIdx].name, op, val)) {
                    match = false;
                    break;
                }
            }
            if (match) {
                for (const auto& [colIdx, valStr] : updates) {
                    if (table.columns[colIdx].type == DataType::INT) {
                        row[colIdx] = std::stoi(valStr);
                    } else {
                        std::string val = valStr;
                        if (val.front() == '"' && val.back() == '"')
                            val = val.substr(1, val.size() - 2);
                        row[colIdx] = val;
                    }
                }
                ++updatedCount;
            }
        }

        fmt::print("{} row(s) updated in {}\n", updatedCount, tableName);
    }


    else {
        fmt::print("Unrecognized query: {}\n", query);
    }
}
