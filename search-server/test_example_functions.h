#pragma once

#include "search_server.h"
#include "request_queue.h"

#include <iostream>

using std::string_literals::operator""s;

// проверка на равенство
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file, const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

// "обертка" для вызова теста по имени функции
template <typename Tfunc>
    void RunTestImpl(Tfunc tfunc, const std::string& func_name) {
    std::cerr << func_name;
    tfunc();
    std::cerr << " OK"s << std::endl;
}

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line, const std::string& hint);

// макросы, упрощающие вызов тестовых функций
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
#define RUN_TEST(func) RunTestImpl((func), #func)

// модульные тесты поисковой системы
void TestExcludeStopWordsFromAddedDocumentContent();
void TestDocumentNotFound();
void TestMatchDocument1();
void TestSortByRelevance();
void TestComputeRating();
void TestSearchWithStatus();
void TestSearchWithPredicate();
void TestPagination();
void TestRequestQueue1();
void TestGetWordFrequencies();
void TestRemoveDocument();

// точка входа
void TestSearchServer();
