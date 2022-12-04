#include <algorithm>
#include <execution>

#include "process_queries.h"

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries) {
    vector<vector<Document>> result(queries.size());
    transform(
        execution::par,
        queries.begin(),
        queries.end(),
        result.begin(),
        [&](const string& raw_query) {
            return search_server.FindTopDocuments(raw_query);
        }
    );
    return result;
}

vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string>& queries) {
    vector<vector<Document>> docsPerQueries = ProcessQueries(search_server, queries);
    vector<Document> result;
    for (const vector<Document>& docs : docsPerQueries) {
        for (const Document& doc : docs) {
            result.push_back(move(doc));
        }
    }
    return result;
}
