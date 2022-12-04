#pragma once

#include "search_server.h"

#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;
private:
    struct QueryResult {
        std::string query;
        std::vector<Document> result;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    int current_time_ = 0;
    int empty_requests_ = 0;
};

// сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> results = search_server_.FindTopDocuments(raw_query, document_predicate);
    requests_.push_back({ raw_query, results });
    ++current_time_;
    if (current_time_ > min_in_day_) {
        current_time_ = min_in_day_;
        if (requests_.front().result.empty()) {
            --empty_requests_;
        }
        requests_.pop_front();
    }
    if (results.empty()) {
        ++empty_requests_;
    }
    return results;
}
