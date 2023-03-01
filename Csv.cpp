#include "Csv.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace errors {
    struct InvalidTableFormat : ErrorInterpreter {
        explicit InvalidTableFormat(std::size_t expectedSize, std::size_t actualSize) :
                ErrorInterpreter("Invalid row size. Expected: " + std::to_string(expectedSize) + ". Actual size: " +
                                 std::to_string(actualSize) + ".") {}
    };

    struct InvalidColumnName : ErrorInterpreter {
        explicit InvalidColumnName(const std::string &name) : ErrorInterpreter(
                "Invalid name of column \"" + name + "\". "
                                                     "Column name must consist of English alphabet letters and be unique.") {}
    };

    struct InvalidRowNumber : ErrorInterpreter {
        explicit InvalidRowNumber(const std::string &name) : ErrorInterpreter("Invalid number of row \"" + name + "\". "
                                                                                                                  "Row number must be a positive integer and be unique.") {}
    };

    struct InvalidFormulaFormat : ErrorInterpreter {
        explicit InvalidFormulaFormat(const std::string &formula) :
                ErrorInterpreter("Invalid formula: " + formula + ". Expected: \"= ARG1 OP ARG2\".") {}
    };

    struct InvalidArgument : ErrorInterpreter {
        explicit InvalidArgument(const std::string &arg) :
                ErrorInterpreter("Invalid argument \"" + arg +
                                 "\" in formula. Argument must be an integer or a cell address: Column_name Row_number.") {}
    };

    struct DivisionByZero : ErrorInterpreter {
        explicit DivisionByZero() : ErrorInterpreter("Trying to divide by zero.") {}
    };

    struct IncorrectFormula : ErrorInterpreter {
        explicit IncorrectFormula() : ErrorInterpreter(
                "Could not compute a cell value. Cell formulas must not refer to each other in a loop.") {}
    };

    struct NotCalculatedValue : ErrorInterpreter {
        explicit NotCalculatedValue(const std::string &columnName, int rowNumber) : ErrorInterpreter(
                "Value in " + columnName
                + std::to_string(rowNumber) + " is not calculated.") {}
    };

    struct NotExpectedValue : ErrorInterpreter {                                            // DEBUG
        explicit NotExpectedValue() : ErrorInterpreter("Operation was not set.") {}
    };
}

namespace csv_interpreter {

    bool is_integer(const std::string &s) {
        if (s.empty()) {
            return false;
        }
        int start = 0;
        if (s[0] == '+' || s[0] == '-') {
            if (s.size() == 1) {
                return false;
            }
            start = 1;
        }
        if (std::all_of(s.begin() + start, s.end(), ::isdigit)) {
            return true;
        }
        return false;
    }

    //NOLINTNEXTLINE
    const std::map<char, Csv::Operation> operationsMap{{'+', Csv::Operation::ADD},
                                                       {'-', Csv::Operation::SUBTRACT},
                                                       {'*', Csv::Operation::MULTIPLY},
                                                       {'/', Csv::Operation::DIVIDE}};

    // returns arg tuple for Cell {isInteger, row_i, column_i} or {isInteger, i, -1}
    std::tuple<bool, int, int> parse_argument(const std::string &arg,
                                              const std::map<std::string, int> &columnIndexes,
                                              const std::map<int, int> &rowIndexes) {
        if (arg.empty()) {
            throw errors::InvalidArgument(arg);
        }
        if (is_integer(arg)) {
            return {true, std::stoi(arg), -1};
        }
        std::string columnName;
        int rowNumber;
        for (int i = 0; i < arg.size(); i++) {
            if (isalpha(arg[i])) {
                columnName += arg[i];
            } else {
                std::string rowStr = arg.substr(i, arg.size() - i);
                if (!std::all_of(rowStr.begin(), rowStr.end(), ::isdigit)) {
                    throw errors::InvalidArgument(arg);
                }
                rowNumber = std::stoi(rowStr);
                break;
            }
        }
        if (columnIndexes.count(columnName) == 0 ||
            columnIndexes.at(columnName) == -1 ||
            rowIndexes.count(rowNumber) == 0) {
            throw errors::InvalidArgument(arg);
        }
        return {false, rowIndexes.at(rowNumber), columnIndexes.at(columnName)};
    }

    Csv::Cell parse_formula(const std::string &formula,
                            const std::map<std::string, int> &columnIndexes,
                            const std::map<int, int> &rowIndexes) {
        Csv::Cell result;
        if (formula.empty() || formula[0] != '=') {
            throw errors::InvalidFormulaFormat(formula);
        }
        std::string arg1, arg2;
        for (int i = 1;
             i < formula.size() - 1; i++) {                          // reading arg1, arg2 and operation symbol
            if (operationsMap.count(formula[i]) != 0) {                         // operation symbol
                if (arg1.empty()) {                                             // arg1 is empty
                    if (formula[i] == '+' ||
                        formula[i] == '-') {               // can be an integer beginning with + or -
                        arg1 += formula[i];
                    } else {
                        throw errors::InvalidFormulaFormat(formula);            // formula begins with / or *
                    }
                } else {                                                        // arg1 is not empty
                    result.op = operationsMap.at(formula[i]);
                    arg2 = formula.substr(i + 1, formula.size() - i - 1);
                    break;
                }
            } else {
                arg1 += formula[i];
            }
        }
        if (result.op == Csv::NOT_SET) {                                        // could not read arg2
            throw errors::InvalidFormulaFormat(formula);
        }
        result.arg1 = parse_argument(arg1, columnIndexes, rowIndexes);
        result.arg2 = parse_argument(arg2, columnIndexes, rowIndexes);
        return result;
    }

    std::vector<std::vector<Csv::Cell>> Csv::parse(const std::vector<std::string> &columns,
                                                   const std::vector<int> &rows,
                                                   const std::vector<std::vector<std::string>> &table) {
        std::map<std::string, int> columnIndexes;           // column name -> column index
        for (int i = 0; i < columns.size(); i++) {
            columnIndexes[columns[i]] = i - 1;
        }
        std::map<int, int> rowIndexes;                      // row number -> row index
        for (int i = 0; i < rows.size(); i++) {
            rowIndexes[rows[i]] = i;
        }
        std::vector<std::vector<Csv::Cell>> result(rows.size(),
                                                   std::vector<Csv::Cell>(columns.size() - 1));  // TODO: check sizes
        for (int i = 1; i < table.size(); i++) {             // filling result table
            for (int j = 1; j < columns.size(); j++) {
                if (is_integer(table[i][j])) {
                    result[i - 1][j - 1] = {true, std::stoi(table[i][j])};
                } else {
                    result[i - 1][j - 1] = parse_formula(table[i][j], columnIndexes, rowIndexes);
                }
            }
        }

        return result;
    }

    void check_dimensions(const std::vector<std::vector<std::string>> &table) {
        auto expectedSize = table[0].size();            // columns number
        for (const auto &row: table) {
            if (expectedSize != row.size()) {
                throw errors::InvalidTableFormat(expectedSize, row.size());
            }
        }
    }

    void check_column_names(const std::vector<std::string> &columns) {
        std::unordered_set<std::string> usedNames;
        for (int i = 0; i < columns.size(); i++) {
            const auto &name = columns[i];
            if (name.empty() && i != 0) {                                // empty cell can be only {0,0}
                throw errors::InvalidColumnName(name);
            }
            if (!std::all_of(name.begin(), name.end(), ::isalpha)) {    // must contain only alphabet letters
                throw errors::InvalidColumnName(name);
            }
            if (usedNames.count(name) != 0) {                            // must be unique
                throw errors::InvalidColumnName(name);
            }
            usedNames.insert(name);
        }
    }

    void check_row_numbers(const std::vector<std::string> &rows) {
        std::unordered_set<int> usedNumbers;
        for (const auto & rowName : rows) {
            if (rowName.empty() || !is_integer(rowName)) {          // must be integer
                throw errors::InvalidRowNumber(rowName);
            }
            int rowNumber = std::stoi(rowName);
            if (rowNumber <= 0) {                                    // must be > 0
                throw errors::InvalidRowNumber(rowName);
            }
            if (usedNumbers.count(rowNumber) != 0) {                 // must be unique
                throw errors::InvalidRowNumber(rowName);
            }
            usedNumbers.insert(rowNumber);
        }
    }

    void Csv::check(const std::vector<std::vector<std::string>> &table) {
        check_dimensions(table);
        check_column_names(table[0]);
        std::vector<std::string> rows(table.size() - 1);
        for (int i = 1; i < table.size(); i++) {
            rows[i - 1] = table[i][0];
        }
        check_row_numbers(rows);
    }

    std::vector<std::string> split_by_comma(const std::string &line) {
        std::stringstream lineStream(line);
        std::vector<std::string> result;
        std::string word;
        while (std::getline(lineStream, word, ',')) {
            word.erase(std::remove_if(word.begin(), word.end(), ::isspace), word.end());
            result.push_back(std::move(word));
        }
        return result;
    }

    void Csv::read(std::fstream &in) {
        std::vector<std::vector<std::string>> table;
        std::string line;
        while (std::getline(in, line)) {             // reading into table
            table.emplace_back(split_by_comma(line));
        }
        if (table.empty()) {
            return;
        }
        check(table);                                      // checking table
        auto columns = std::move(table[0]);              // column names
        table[0].clear();
        std::vector<int> rows(table.size() - 1);        // row numbers
        for (int i = 1; i < table.size(); i++) {
            rows[i - 1] = std::stoi(table[i][0]);
        }
        auto parsedTable = parse(columns, rows, table);
        mColumns = std::move(columns);
        mRows = std::move(rows);
        mTable = std::move(parsedTable);
    }

    int Csv::calc(int i, int j, std::vector<std::vector<bool>> &marked) {
        auto &cell = mTable[i][j];
        if (cell.isCalculated) {
            return cell.calculatedValue;
        }
        if (marked[i][j]) {
            throw errors::IncorrectFormula();
        }
        marked[i][j] = true;
        int val1, val2;
        if (std::get<0>(cell.arg1)) {            // is integer
            val1 = std::get<1>(cell.arg1);
        } else {                                    // needs to be computed
            val1 = calc(std::get<1>(cell.arg1), std::get<2>(cell.arg1), marked);
        }
        if (std::get<0>(cell.arg2)) {            // is integer
            val2 = std::get<1>(cell.arg2);
        } else {                                    // needs to be computed
            val2 = calc(std::get<1>(cell.arg2), std::get<2>(cell.arg2), marked);
        }
        switch (cell.op) {
            case ADD:
                cell = {true, val1 + val2};
                break;
            case SUBTRACT:
                cell = {true, val1 - val2};
                break;
            case MULTIPLY:
                cell = {true, val1 * val2};
                break;
            case DIVIDE: {
                if (val2 == 0) {
                    throw errors::DivisionByZero();
                }
                cell = {true, val1 / val2};
                break;
            }
            case NOT_SET:
                throw errors::NotExpectedValue();
        }
        return cell.calculatedValue;
    }

    void Csv::compute() {
        if (mTable.empty() || mTable[0].empty()) {
            return;
        }
        auto rowsNumber = mTable.size();
        auto columnsNumber = mTable[0].size();
        std::vector<std::vector<bool>> marked(rowsNumber, std::vector<bool>(columnsNumber));
        for (int i = 0; i < rowsNumber; i++) {
            for (int j = 0; j < columnsNumber; j++) {
                auto &cell = mTable[i][j];
                if (cell.isCalculated) {
                    continue;
                }
                cell.calculatedValue = calc(i, j, marked);
                cell.isCalculated = true;
            }
        }
    }

    void Csv::print(std::ostream &out) const {
        for (int i = 0; i < mColumns.size(); i++) {
            out << mColumns[i];
            if (i != mColumns.size() - 1) {
                out << ',';
            }
        }
        out << '\n';
        for (int i = 0; i < mTable.size(); i++) {
            out << mRows[i];
            for (int j = 0; j < mTable[i].size(); j++) {
                auto cell = mTable[i][j];
                if (!cell.isCalculated) {
                    throw errors::NotCalculatedValue(mColumns[j + 1], mRows[i]);
                }
                out << ',' << cell.calculatedValue;
            }
            out << '\n';
        }
    }

    std::fstream &operator>>(std::fstream &in, Csv &csvToRead) {
        csvToRead.read(in);
        return in;
    }

    std::ostream &operator<<(std::ostream &out, const Csv &csvToPrint) {
        csvToPrint.print(out);
        return out;
    }
} // namespace csv_interpreter

