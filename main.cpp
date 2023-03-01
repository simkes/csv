#include "Csv.h"
#include <iostream>
#include <fstream>

int main(int argc, char *argv[]) {
    if(argc != 2) {
        std::cout << "Incorrect input. CSV-format file should be provided.\n";
        return 0;
    }
    std::string fileName = argv[1];
    std::fstream file (fileName, std::ios::in);
    if(!file.is_open()) {
        std::cout << "Could not open file \"" + fileName + "\".\n";
        return 0;
    }

    csv_interpreter::Csv csvTable;
    try {
        file >> csvTable;
        csvTable.compute();
        std::cout << csvTable;
    } catch (errors::ErrorInterpreter &e) {
        std::cout << e.what() << '\n';
    } catch (std::ios_base::failure &) {
        std::cout << "Invalid file format\n";
    } catch (std::out_of_range &) {
        std::cout << "Number entered was out of range.\n";
    }

    file.close();
}
