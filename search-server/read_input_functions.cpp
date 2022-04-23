#include "read_input_functions.h"

using namespace std;

// считывает строку целиком, от начала до символа новой строки
string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

// считывает целое число (количество документов)
int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}