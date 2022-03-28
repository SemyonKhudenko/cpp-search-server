#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    vector<int> duplicate_ids;
    set<vector<string>> set_document_words;
    vector<string> document_words;
    for (const auto& document_id : search_server) {
        document_words.clear();

        for (const auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
            document_words.push_back(word);
        }

        if (set_document_words.find(document_words) == set_document_words.end()) {
            set_document_words.emplace(document_words);
        } else {
            duplicate_ids.push_back(document_id);
        }
    }

    for (const auto& document_id : duplicate_ids) {
        cout << "Found duplicate document id " << document_id << endl;
        search_server.RemoveDocument(document_id);
    }
}
