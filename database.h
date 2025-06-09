#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

enum class DataType { INT, STRING };

using DataValue = std::variant<int, std::string>;
using Row = std::vector<DataValue>;

struct Column {
    std::string name;
    DataType type;
};

struct Table {
    std::string name;
    std::vector<Column> columns;
    std::vector<Row> rows;
};

class Database {
public:
    void execute(const std::string& query);
private:
    std::unordered_map<std::string, Table> tables;

    DataType parseType(const std::string& typeStr);
    bool evaluateCondition(const Table& table, const Row& row,
                           const std::string& colName, const std::string& op, const std::string& valueStr);
    int getColumnIndex(const Table& table, const std::string& columnName);
};
