#include "string_processing.h"

using namespace std;

// возвращает вектор, содержащий все слова из переданной строки
vector<string_view> SplitIntoWords(string_view text) {
    vector<string_view> result;
    while (true) {
        auto pos_space = text.find(' ');
        result.push_back(text.substr(0, pos_space));
        if (pos_space == text.npos) {
            break;
        } else {
            text.remove_prefix(pos_space + 1);
        }
    }
    return result;
}