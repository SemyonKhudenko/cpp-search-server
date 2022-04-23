#include "test_example_functions.h"

using namespace std;

// проверка на истинность
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
void TestMatchDocument1() {
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

// тест проверяет, что поисковая система правильно разбивает поисковую выдачу на страницы
void TestPagination() {
    SearchServer server("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, {9});
    server.AddDocument(4, "лось валера"s, DocumentStatus::ACTUAL, {1, 2, 5});
    auto search_result = server.FindTopDocuments("ухоженный кот валера"s);
    // разбиваем поисковую выдачу на страницы по 2 результата на каждой
    const auto pages = Paginate(search_result, 2);
    int pages_count = distance(pages.begin(), pages.end());
    ASSERT_EQUAL_HINT(pages_count, 3, "Pagination error"s);
}

// тест проверяет, что очередь запросов в поисковой системе работает правильно
void TestRequestQueue1() {
    SearchServer server("and in at"s);
    RequestQueue request_queue(server);
    server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    ASSERT_EQUAL(request_queue.GetNoResultRequests(), 1437);
}

// тест проверяет, что поисковая система правильно вычисляет частоту слов в документе
void TestGetWordFrequencies() {
    SearchServer server(""s);
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    const auto result = server.GetWordFrequencies(1);
    // в документе содержатся три уникальных слова
    ASSERT_EQUAL_HINT(result.size(), 3, "Unique words count is incorrect"s);
    // частота слова "пушистый": 2 (раза встречается слово) / 4 (всего слов в документе) = 0.5
    ASSERT_EQUAL_HINT(result.at("пушистый"s), 0.5, "Frequency calculation is wrong"s);
}

void TestRemoveDocument() {
    SearchServer server("и в на"s);
    server.AddDocument(1, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(2, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(3, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(4, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, {9});
    server.AddDocument(5, "лось валера"s, DocumentStatus::ACTUAL, {1, 2, 5});
    server.AddDocument(6, "североамериканский кролик-зануда"s, DocumentStatus::ACTUAL, {1, 2, 2});
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 6, "Wrong documents count"s);
    server.RemoveDocument(0);
    server.RemoveDocument(7);
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 6, "No document should be removed at this point"s);
    auto search_result = server.FindTopDocuments("белый пушистый ухоженный североамериканский кот валера"s);
    // матчинг есть во всех документах, должны вернуться MAX_RESULT_DOCUMENT_COUNT наиболее релевантных
    ASSERT_EQUAL_HINT(search_result.size(), MAX_RESULT_DOCUMENT_COUNT, "MAX_RESULT_DOCUMENT_COUNT documents should be found"s);
    // удаляем наименее релевантный документ,
    server.RemoveDocument(3);
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 5, "One document should be removed at this point"s);
    search_result = server.FindTopDocuments("белый пушистый ухоженный североамериканский кот валера"s);
    // матчинг есть во всех документах, все еще должны вернуться MAX_RESULT_DOCUMENT_COUNT наиболее релевантных
    ASSERT_EQUAL_HINT(search_result.size(), MAX_RESULT_DOCUMENT_COUNT, "MAX_RESULT_DOCUMENT_COUNT documents should be found"s);
    // удаляем наиболее релевантный документ,
    server.RemoveDocument(2);
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 4, "Two documents should be removed at this point"s);
    search_result = server.FindTopDocuments("белый пушистый ухоженный североамериканский кот валера"s);
    // должны вернуться оставшиеся 4 документа
    ASSERT_EQUAL_HINT(search_result.size(), 4, "4 documents should be found"s);
}

// точка входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestDocumentNotFound);
    RUN_TEST(TestMatchDocument1);
    RUN_TEST(TestSortByRelevance);
    RUN_TEST(TestComputeRating);
    RUN_TEST(TestSearchWithStatus);
    RUN_TEST(TestSearchWithPredicate);
    RUN_TEST(TestPagination);
    RUN_TEST(TestRequestQueue1);
    RUN_TEST(TestGetWordFrequencies);
    RUN_TEST(TestRemoveDocument);
    cout << "Search server testing finished"s << endl << endl;
}