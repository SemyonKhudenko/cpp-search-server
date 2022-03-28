#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor
                                                     // from string container
{
}

// добавляет сведения о документе в хранилище
void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_id_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.push_back(document_id);
}

// возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска с фильтрацией по статусу
vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

// возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска
vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

// возвращает общее количество документов
int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

// возвращает все плюс-слова запроса, содержащиеся в документе и статус документа
tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);

    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

// возвращает итератор, указывающий на id первого документа, хранящегося в поисковом сервере
const vector<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

// возвращает итератор, указывающий на id последнего документа, хранящегося в поисковом сервере
const vector<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

// возвращает частоты слов в документе с данным id
const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string, double> empty_result;
    if (document_id_to_word_freqs_.count(document_id) == 0) {
        return empty_result;
    }
    return document_id_to_word_freqs_.at(document_id);
}

// удаляет документ из поискового сервера по id
void SearchServer::RemoveDocument(int document_id) {
    const auto document_id_it = find(document_ids_.begin(), document_ids_.end(), document_id);
    if (document_id_it == document_ids_.end()) {
        return;
    }
    document_ids_.erase(document_id_it);
    documents_.erase(document_id);
    document_id_to_word_freqs_.erase(document_id);
    for (auto& [word, item] : word_to_document_freqs_) {
        item.erase(document_id);
    }
}

// проверяет, является ли слово стоп-словом
bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

// проверяет, не содержит ли слово недопустимые символы
bool SearchServer::IsValidWord(const string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

// возвращает вектор, содержащий все слова из переданной строки за исключением стоп-слов
vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + word + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

// возвращает среднее значение из вектора рейтингов
int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
	if (ratings.empty()) {
		return 0;
	}
	return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

// парсинг одного слова поискового запроса
SearchServer::QueryWord SearchServer::ParseQueryWord(const string& text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + text + " is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}

// парсинг поискового запроса
SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    Query result;
    for (const string& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            } else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}

// рассчитывает IDF слова
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

// выводит результаты поиска в консоль
void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

// добавление нового документа в поисковый сервер
void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
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
