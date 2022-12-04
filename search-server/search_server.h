#pragma once

#include "document.h"
#include "paginator.h"
#include "string_processing.h"
#include "concurrent_map.h"

#include <cmath>
#include <execution>
#include <map>
#include <numeric>
#include <set>
#include <unordered_set>
#include <vector>

using namespace std::string_literals;

// максимальное число документов в поисковой выдаче
const int MAX_RESULT_DOCUMENT_COUNT = 5;

// точность для переменных с плавающей точкой
const double EPSILON = 1e-6;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view stop_words_text);

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentStatus status) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const;

    const std::set<int>::const_iterator begin() const;
    const std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double, std::less<>>& GetWordFrequencies(int document_id) const;

    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);
    void RemoveDocument(int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::unordered_set<std::string> words_values;
    };

    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double, std::less<>>> document_id_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(const std::string_view word) const;
    static bool IsValidWord(const std::string_view word);
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view text, bool uniquify = false) const;
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;
};

// конструктор-шаблон, принимающий на вход произвольный контейнер строк
template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

// возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска с фильтрацией посредством функции-предиката
// версия без ExecutionPolicy просто вызывает последовательную
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

// возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска с фильтрацией посредством функции-предиката
template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
	const auto query = ParseQuery(raw_query, true);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);
    std::sort(
        policy,
        matched_documents.begin(),
        matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        }
    );
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

// возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска с фильтрацией по статусу
template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

// возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска
template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query) const {
	return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

// возвращает все результаты поиска с фильтрацией посредством функции-предиката
// версия с не определенной ExecutionPolicy просто вызывает последовательную
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

// возвращает все результаты поиска с фильтрацией посредством функции-предиката
// последовательная версия
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

// возвращает все результаты поиска с фильтрацией посредством функции-предиката
// параллельная версия
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const {
    constexpr size_t THREAD_COUNT = 101;
    ConcurrentMap<int, double> mt_document_to_relevance(THREAD_COUNT);
    std::for_each(
        std::execution::par,
        query.plus_words.begin(),
        query.plus_words.end(),
        [this, &document_predicate, &mt_document_to_relevance](std::string_view word) {
            if (word_to_document_freqs_.count(word) == 0) {
                return;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    mt_document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
    );
    std::map<int, double> document_to_relevance(mt_document_to_relevance.BuildOrdinaryMap());
    std::for_each(
    	std::execution::par,
		query.minus_words.begin(),
		query.minus_words.end(),
		[this, &document_predicate, &document_to_relevance](std::string_view word){
			if (word_to_document_freqs_.count(word) != 0) {
				for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
					document_to_relevance.erase(document_id);
				}
			}
    	}
	);
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

// удаляет документ из поискового сервера по id
template <typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
    if (document_ids_.count(document_id) == 0) {
        return;
    }
    std::map<std::string_view, double, std::less<>>& erase_map = document_id_to_word_freqs_.at(document_id);
    std::vector<std::string_view> words(erase_map.size());
    std::transform(
        policy,
        erase_map.begin(),
        erase_map.end(),
        words.begin(),
        [](const auto item) { return item.first; }
    );
    std::for_each(
        policy,
        words.begin(),
        words.end(),
        [&](const auto& word) {
            word_to_document_freqs_.at(word).erase(document_id);
        }
    );
    document_ids_.erase(document_id);
    documents_.erase(document_id);
    document_id_to_word_freqs_.erase(document_id);
}

void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view> words, DocumentStatus status);
void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);
void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);
void MatchDocuments(const SearchServer& search_server, const std::string& query);
