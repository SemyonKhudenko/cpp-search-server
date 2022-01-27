#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

// максимальное число документов в поисковой выдаче
const int MAX_RESULT_DOCUMENT_COUNT = 5;

// считывает строку целиком, от начала до символа новой строки
string ReadLine() {
    string str;
    getline(cin, str);
    return str;
}

// считывает целое число (количество документов)
int ReadLineWithNumber() {
    int num;
    cin >> num;
    ReadLine();
    return num;
}

// возвращает вектор, содержащий все слова из переданной строки
vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

// формат, в котором  возвращаются результаты поиска
struct Document {
    int id;
    double relevance;
    int rating;
};

// возможные статусы документов
enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
	// возвращает общее количество документов
    int GetDocumentCount() const {
        return documents_.size();
    }

	// заполняет множество (set) стоп-слов, поиск по которым не производится
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    // добавляет сведения о документе в хранилище
    void AddDocument(int document_id,
					 const string& document,
					 DocumentStatus status,
					 const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inverted_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inverted_word_count;
        }
        documents_.emplace(document_id,
            DocumentData {
                ComputeAverageRating(ratings),
                status
            });
    }

    // возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска
    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    // возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска с фильтрацией по статусу
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query,
        		[status](int document_id, DocumentStatus document_status, int rating) {
        				return document_status == status;
        		});
    }

    // возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска с фильтрацией посредством функции-предиката
    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
        		if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    // возвращает все плюс-слова запроса, содержащиеся в документе и статус документа
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
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

private:
    // сведения о рейтинге и статусе документа
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    // коллекция содержит слова, поиск по которым не осуществляется
    set<string> stop_words_;

    // словарь, содержащий частоту появления слова в каждом документе
    map<string, map<int, double>> word_to_document_freqs_;

    // словарь, содержащий сведения о рейтинге и статусе каждого документа
    map<int, DocumentData> documents_;

    // проверка, является ли слово стоп-словом
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    // возвращает вектор, содержащий все слова из переданной строки за исключением стоп-слов
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    // возвращает среднее значение из вектора рейтингов
    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
    }

    // структура одного слова поискового запроса
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    // парсинг одного слова поискового запроса
    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    // структура поискового запроса после парсинга
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    // парсинг поискового запроса
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // поиск всех документов, соответствующих поисковому запросу
    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance; // здесь сохраним релевантность каждого документа
        // считаем релевантность для каждого документа, в котором встречается плюс-слово и добавляем в словарь
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double idf = log(static_cast<double>(GetDocumentCount()) / word_to_document_freqs_.at(word).size());
            for (const auto [document_id, tf] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += tf * idf;
                }
            }
        }
        // убираем из словаря документы, в которых встречаются минус-слова
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
        // собираем и возвращаем результат
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }
};


// проверка работоспособности кода

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}

/*
* ВЫВОД
* ACTUAL by default:
* { document_id = 1, relevance = 0.866434, rating = 5 }
* { document_id = 0, relevance = 0.173287, rating = 2 }
* { document_id = 2, relevance = 0.173287, rating = -1 }
* BANNED:
* { document_id = 3, relevance = 0.231049, rating = 9 }
* Even ids:
* { document_id = 0, relevance = 0.173287, rating = 2 }
* { document_id = 2, relevance = 0.173287, rating = -1 }
*/
