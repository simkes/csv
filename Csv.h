#ifndef CSV_H
#define CSV_H

#include <vector>
#include <iosfwd>
#include <map>
#include <string>
#include <stdexcept>

namespace errors {
    /*
     Base class of interpreting errors
     */
    struct ErrorInterpreter : std::runtime_error {
        explicit ErrorInterpreter(const std::string &message)
                : std::runtime_error(message) {
        }
    };
}

namespace csv_interpreter {

    class Csv {
    public:
        enum Operation {
            ADD, SUBTRACT, MULTIPLY, DIVIDE, NOT_SET
        };

        /*
         table consists of Cells
         which can be either calculated value (integer) or formula (ARG1 OP ARG2)
         */
        struct Cell {
            bool isCalculated = false;
            int calculatedValue = 0;
            std::tuple<bool, int, int> arg1;       // {isInteger, row_i , column_i} : indexes of arg in table
            std::tuple<bool, int, int> arg2;       // or {isInteger, i, -1} : integer i
            Operation op = NOT_SET;
        };


        /*
        calculates Cell values into calculatedValue field using formulas
        calls calc() (private)
        */
        void compute();

        /*
         overloaded operator of reading from std::fstream
         */
        friend std::fstream &operator>>(std::fstream &in, Csv &csvToRead);

        /*
         overloaded operator of writing to std::ostream
         */
        friend std::ostream &operator<<(std::ostream &out, const Csv &csvToPrint);

    private:

        std::vector<std::string> mColumns; // column names
        std::vector<int> mRows;            // row numbers
        std::vector<std::vector<Cell>> mTable;


        /*
         parses cells content
         throws InvalidFormulaFormat or InvalidArgument
         converts cell  string-content into Cell structure
         returns parsed table
         */
        std::vector<std::vector<Cell>> parse(const std::vector<std::string> &columns,
                   const std::vector<int> &rows,
                   const std::vector<std::vector<std::string>> &table);

        /*
         checks dimension of table (throws InvalidTableFormat)
         checks names of columns and numbers of rows (throws InvalidColumnName)
         */
        void check(const std::vector<std::vector<std::string>> &table);

        /*
         reads csv-format file into two-dimensional vector of string (separating by commas)
         calls check() and parse() inside
         */
        void read(std::fstream &in);

        /*
         returns value of mTable[i][j]
         if it is not calculated goes in recursion
         throws IncorrectFormula if cells refer to each other in a loop
         throws DivisionByZero error
         */
        int calc(int i, int j, std::vector<std::vector<bool>> &marked);


        /*
         prints csv-format table in std::ostream
         throws NotCalculatedValue if value of cell has not been calculated
         */
        void print(std::ostream &out) const;
    };

} // namespace csv_interpreter

#endif //CSV_H
