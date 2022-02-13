#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <iostream>
#include <numeric>


using namespace std;

// максимальное число документов в поисковой выдаче
const int MAX_RESULT_DOCUMENT_COUNT = 5;

// точность для переменных с плавающей точкой
const double EPSILON = 1e-6;

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
    Document() = default;
    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }
    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

// возможные статусы документов
enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

// проверка, не содержит ли слово недопустимые символы
static bool IsValidWord(const string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

// возвращает словарь, не содержащий пустых строк
template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!IsValidWord(str)) {
            throw invalid_argument("stop word must not contain non-printable characters");
        }
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

class SearchServer {
public:
    // конструктор, принимающий на вход контейнер, содержащий список стоп-слов
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {}

    // конструктор, принимающий на вход список стоп-слов в виде строки
    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {}

	// возвращает общее количество документов
    int GetDocumentCount() const {
        return documents_.size();
    }

    // добавляет сведения о документе в хранилище
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0) {
            throw invalid_argument("document id must not be negative");
        }
        if (documents_.count(document_id) > 0) {
            throw invalid_argument("document id already exists");
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inverted_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inverted_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
        document_ids_.push_back(document_id);
    }

    // возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска
    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    // возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска с фильтрацией по статусу
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int, DocumentStatus document_status, int) {
        	return document_status == status;
        });
    }

    // возвращает первые MAX_RESULT_DOCUMENT_COUNT результатов поиска с фильтрацией посредством функции-предиката
    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        	if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
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

    // возвращает идентификатор документа по его порядковому номеру в хранилище
    int GetDocumentId(int index) const {
        if (index < 0 || index >= GetDocumentCount()) {
            throw out_of_range("document index is out of range");
        }
        return document_ids_[index];
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

    // вектор, содержащий идентификаторы документов
    vector<int> document_ids_;

    // проверка, является ли слово стоп-словом
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    // возвращает вектор, содержащий все слова из переданной строки за исключением стоп-слов
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsValidWord(word)) {
                throw invalid_argument("document word must not contain non-printable characters");
            }
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
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
            throw invalid_argument("invalid word in search query");
        }
        return {text, is_minus, IsStopWord(text)};
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

// -------- Тестовый фреймворк: реализация макросов ASSERT* и RUN_TEST --------
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename Tfunc>
void RunTestImpl(Tfunc tfunc, const string& func_name) {
    cerr << func_name;
    tfunc();
    cerr << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// -------- начало модульных тестов поисковой системы ----------

// тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// тест проверяет, что поисковая система не возвращает результаты для отсутствующих слов и минус-слов
void TestDocumentNotFound() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    // поиск по отсутствующему в документе слову не должен возвращать результатов
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("platypus"s).empty(), "Non-matching word must not return any result"s);
    }

    // поиск по минус-слову не должен возвращать результатов, даже если в запросе есть любые другие совпадающие слова
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("cat -city"s).empty(), "Matching minus word must not return any result"s);
    }
}

// тест проверяет, что матчинг возвращает все присутствующие в документе слова запроса и не возвращает отсутствующие
void TestDocumentMatch() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("cat city platypus"s, 42);
        ASSERT_EQUAL_HINT(words.size(), 2, "Wrong count of matching words"s);
        ASSERT_EQUAL_HINT(count(words.begin(), words.end(), "cat"s), 1, "Missing matching word"s);
        ASSERT_EQUAL_HINT(count(words.begin(), words.end(), "city"s), 1, "Missing matching word"s);
        ASSERT_EQUAL_HINT(count(words.begin(), words.end(), "platypus"s), 0, "Wrong match"s);
    }
}

// тест проверяет, что поисковая система правильно вычисляет релевантность и правильно сортирует результаты
void TestSortByRelevance() {
    SearchServer server("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});
    auto search_result = server.FindTopDocuments("пушистый ухоженный кот"s);

    // проверка вычисления релевантности
    ASSERT_HINT(abs(search_result[0].relevance - 0.866434) < EPSILON, "Relevance calculation error"s);
    ASSERT_HINT(abs(search_result[2].relevance - 0.173287) < EPSILON, "Relevance calculation error"s);

    // проверка сортировки поисковой выдачи по убыванию релевантности
    ASSERT_EQUAL_HINT(search_result[0].id, 1, "Wrong document sorting order"s);
    ASSERT_EQUAL_HINT(search_result[1].id, 0, "Wrong document sorting order"s);
    ASSERT_EQUAL_HINT(search_result[2].id, 2, "Wrong document sorting order"s);
}

// тест проверяет, что поисковая система правильно вычисляет рейтинг документа
void TestComputeRating() {
    SearchServer server(""s);
    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, 3, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    auto search_result = server.FindTopDocuments("пушистый ухоженный кот"s);

    // если рейтингов у документа нет, то средний рейтинг должен быть равен 0
    ASSERT_EQUAL_HINT(search_result[0].rating, 0, "Rating calculation error"s);

    // общий случай, отрицательный средний рейтинг
    ASSERT_EQUAL_HINT(search_result[1].rating, -1, "Rating calculation error"s);

    // случай, когда сумма рейтингов не делится без остатка на их количество, должна возвращаться целая часть частного
    ASSERT_EQUAL_HINT(search_result[2].rating, 2, "Rating calculation error"s);
}

// тест проверяет, что поисковая система правильно учитывает статусы документов
void TestSearchWithStatus() {
    SearchServer server(""s);
    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::IRRELEVANT, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::REMOVED, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

    // поиск без явно указанного рейтинга не должен возвращать документы с рейтингом не равным умолчанию (ACTUAL)
    ASSERT_HINT(server.FindTopDocuments("пушистый ухоженный крот"s).empty(), "No results must be returned for the query"s);

    // при явном указании рейтинга поиск должен возвращать подходящий документ
    auto search_result = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL_HINT(search_result.size(), 1, "Found documents count is incorrect"s);
    ASSERT_EQUAL_HINT(search_result[0].id, 0, "Wrong document found"s);
    search_result = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL_HINT(search_result.size(), 1, "Found documents count is incorrect"s);
    ASSERT_EQUAL_HINT(search_result[0].id, 1, "Wrong document found"s);
    search_result = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::REMOVED);
    ASSERT_EQUAL_HINT(search_result.size(), 1, "Found documents count is incorrect"s);
    ASSERT_EQUAL_HINT(search_result[0].id, 2, "Wrong document found"s);
    search_result = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
    ASSERT_EQUAL_HINT(search_result.size(), 1, "Found documents count is incorrect"s);
    ASSERT_EQUAL_HINT(search_result[0].id, 3, "Wrong document found"s);
}

// тест проверяет, что поисковая система правильно выполняет поиск с использованием пользовательского предиката
void TestSearchWithPredicate() {
    SearchServer server("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});
    auto search_result = server.FindTopDocuments("пушистый ухоженный кот"s,
        // предикат: только актуальные документы, только отрицательный рейтинг, только четные document_id
        [](int document_id, DocumentStatus status, int rating) {
            return status == DocumentStatus::ACTUAL && rating < 0 && document_id % 2 == 0;
        });
    ASSERT_EQUAL_HINT(search_result.size(), 1, "Found documents count is incorrect"s);
    ASSERT_EQUAL_HINT(search_result[0].id, 2, "Wrong document found"s);
}

// тест проверяет, что попытка передать стоп-слово, содержащее недопустимые символы при инициализации поискового сервера вызывает ожидаемую исключительную ситуацию
void TestExceptionInvalidStopWord() {
    try
    {
        SearchServer server("и в н\x12а"s);
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "stop word must not contain non-printable characters"s);
    }
}

// тест проверяет, что попытка добавить в хранилище документ, содержащий недопустимые символы, вызывает ожидаемую исключительную ситуацию
void TestExceptionInvalidDocumentWord() {
    try
    {
        SearchServer server(""s);
        server.AddDocument(3, "большой скво\x12рец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "document word must not contain non-printable characters"s);
    }
}

// тест проверяет, что попытка добавить в хранилище документ с отрицательным id вызывает ожидаемую исключительную ситуацию
void TestExceptionNegativeDocumentId() {
    try
    {
        SearchServer server(""s);
        server.AddDocument(-3, "большой скворец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "document id must not be negative"s);
    }
}

// тест проверяет, что попытка добавить в хранилище документ с уже существующим id вызывает ожидаемую исключительную ситуацию
void TestExceptionDocumentIdExists() {
    try
    {
        SearchServer server(""s);
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "document id already exists"s);
    }
}

// тест проверяет, что наличие ошибок в поисковом запросе вызывает ожидаемую исключительную ситуацию
void TestExceptionInvalidSearchQuery() {
    SearchServer server(""s);
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    // наличие специальных символов в запросе должно выбрасывать исключение
    try
    {
        server.FindTopDocuments("пушис\x12тый"s);
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid word in search query"s);
    }
    // наличие более одного минуса перед словом должно выбрасывать исключение
    try
    {
        server.FindTopDocuments("пушистый -кот"s);
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid word in search query"s);
    }
    try
    {
        server.FindTopDocuments("пушистый -----кот"s);
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid word in search query"s);
    }
    // наличие обособленного минуса должно выбрасывать исключение
    try
    {
        server.FindTopDocuments("пушистый -"s);
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid word in search query"s);
    }
    try
    {
        server.FindTopDocuments("пушистый - кот"s);
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid word in search query"s);
    }
}

// тест проверяет, что попытка найти id документа с номером, выходящим за пределы размера хранилища, вызывает ожидаемую исключительную ситуацию
void TestExceptionDocumentIndexIsOutOfRange() {
    SearchServer server(""s);
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    try
    {
        server.GetDocumentId(-1);
    }
    catch(const out_of_range& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "document index is out of range"s);
    }
    try
    {
        server.GetDocumentId(8);
    }
    catch(const out_of_range& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "document index is out of range"s);
    }
}

// функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestDocumentNotFound);
    RUN_TEST(TestDocumentMatch);
    RUN_TEST(TestSortByRelevance);
    RUN_TEST(TestComputeRating);
    RUN_TEST(TestSearchWithStatus);
    RUN_TEST(TestSearchWithPredicate);
    RUN_TEST(TestExceptionInvalidStopWord);
    RUN_TEST(TestExceptionInvalidDocumentWord);
    RUN_TEST(TestExceptionNegativeDocumentId);
    RUN_TEST(TestExceptionDocumentIdExists);
    RUN_TEST(TestExceptionInvalidSearchQuery);
    RUN_TEST(TestExceptionDocumentIndexIsOutOfRange);
}

// --------- окончание модульных тестов поисковой системы -----------

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

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

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;

    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, {1, 1, 1});

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
}
