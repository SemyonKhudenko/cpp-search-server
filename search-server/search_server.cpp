#include "search_server.h"

using namespace std;

// конструктор, принимающий на вход std::string
SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(string_view(stop_words_text))
{
}

// конструктор, принимающий на вход std::string_view
SearchServer::SearchServer(const string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}

// добавляет сведения о документе в хранилище
void SearchServer::AddDocument(int document_id, const string_view document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const string_view word : words) {
        auto it = words_values_.emplace(string(word));
        string_view word_view = *it.first;
        word_to_document_freqs_[word_view][document_id] += inv_word_count;
        document_id_to_word_freqs_[document_id][word_view] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

// возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска с фильтрацией по статусу
// версия с не определенной ExecutionPolicy просто вызывает последовательную
vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

// возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска с фильтрацией по статусу
// последовательная версия
vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

// возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска с фильтрацией по статусу
// параллельная версия
vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::par, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

// возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска
// версия с не определенной ExecutionPolicy просто вызывает последовательную
vector<Document> SearchServer::FindTopDocuments(const string_view raw_query) const {
    return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
}

// возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска
// последовательная версия
vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, const string_view raw_query) const {
    return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
}

// возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска
// параллельная версия
vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, const string_view raw_query) const {
    return FindTopDocuments(execution::par, raw_query, DocumentStatus::ACTUAL);
}

// возвращает общее количество документов
int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

// возвращает все плюс-слова запроса, содержащиеся в документе и статус документа
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::sequenced_policy&, const string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);
    for (const string_view word : query.minus_words) {
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { vector<string_view>(), documents_.at(document_id).status };
        }
    }
    vector<string_view> matched_words;
    matched_words.reserve(query.plus_words.size());
    for_each(query.plus_words.begin(), query.plus_words.end(),
        [this, document_id, &matched_words](auto& word) {
            if (document_id_to_word_freqs_.at(document_id).count(word) != 0) {
                matched_words.push_back(word);
            }
        });
    return { matched_words, documents_.at(document_id).status };
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::parallel_policy&, const string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);
    for (const string_view word : query.minus_words) {
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { vector<string_view>(), documents_.at(document_id).status };
        }
    }
    vector<string_view> matched_words;
    matched_words.reserve(query.plus_words.size());
 
    for_each(execution::par, query.plus_words.begin(), query.plus_words.end(),
        [this, document_id, &matched_words](auto& word) {
            if (document_id_to_word_freqs_.at(document_id).count(word) != 0) {
                matched_words.push_back(word);
            }
        });
   const auto word_check = [this, document_id](string_view word){
        auto f = word_to_document_freqs_.find(word);
        return f != word_to_document_freqs_.end() && f->second.count(document_id);
    };
    auto word_end = copy_if(execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), word_check);
    sort(execution::par, matched_words.begin(), word_end);
    matched_words.erase(unique(execution::par, matched_words.begin(), word_end), matched_words.end());
    return { matched_words, documents_.at(document_id).status };
}

// возвращает итератор, указывающий на id первого документа, хранящегося в поисковом сервере
const set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

// возвращает итератор, указывающий на id последнего документа, хранящегося в поисковом сервере
const set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

// возвращает частоты слов в документе с данным id
const map<string_view, double, less<>>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double, less<>> empty_result;
    if (document_id_to_word_freqs_.count(document_id) == 0) {
        return empty_result;
    }
    return document_id_to_word_freqs_.at(document_id);
}

// удаляет документ из поискового сервера по id
void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(execution::seq, document_id);
}

// проверяет, является ли слово стоп-словом
bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(word) > 0;
}

// проверяет, не содержит ли слово недопустимые символы
bool SearchServer::IsValidWord(const string_view word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

// возвращает вектор, содержащий все слова из переданной строки за исключением стоп-слов
vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> result;
    for (const string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            result.push_back(word);
        }
    }
    return result;
}

// возвращает среднее значение из вектора рейтингов
int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
	if (ratings.empty()) {
		return 0;
	}
	return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

// парсинг одного слова поискового запроса
SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text.remove_prefix(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw invalid_argument("Query word "s + string(text) + " is invalid"s);
    }
    return {text, is_minus, IsStopWord(text)};
}

// парсинг поискового запроса
SearchServer::Query SearchServer::ParseQuery(const string_view text) const {
    Query result;
    for (const string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    sort(execution::par, result.minus_words.begin(), result.minus_words.end());
    result.minus_words.erase(unique(execution::par, result.minus_words.begin(), result.minus_words.end()), result.minus_words.end());
    sort(execution::par, result.plus_words.begin(), result.plus_words.end());
    result.plus_words.erase(unique(execution::par, result.plus_words.begin(), result.plus_words.end()), result.plus_words.end());
    return result;
}

// рассчитывает IDF слова
double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

// выводит результаты поиска в консоль
void PrintMatchDocumentResult(int document_id, const vector<string_view> words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string_view word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

// добавление нового документа в поисковый сервер
void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const invalid_argument& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

// поиск в документах сервера по запросу
void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const invalid_argument& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

// матчинг документов по запросу
void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        for (const auto& document_id : search_server) {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const invalid_argument& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}
