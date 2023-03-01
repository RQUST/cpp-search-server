#pragma once

#include "process_queries.h"
#include "remove_duplicates.h"
#include "request_queue.h"
#include "search_server.h"
#include "paginator.h"

#include <iomanip>
#include <iostream>
#include <map>
#include <map>
#include <list>
#include <vector>

using std::string_literals::operator""s;
using namespace std;

template <typename T>
void Print(std::ostream& out, T container) 
{
    int size = container.size();

    for (auto element : container) 
    {
        --size;

        if (size > 0) {  out << element << ", "s;  }
        else {   out << element;  }
    }
}

// ������ ��� ������ �������
template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& container)
{
    out << "["s;
    Print(out, container);
    out << "]"s;

    return out;
}
 
template <typename T>
std::ostream& operator<<(std::ostream& out, const std::set<T>& container)
{
    out << "{"s;
    Print(out, container);
    out << "}"s;

    return out;
}
 
template <typename Key, typename Value>
std::ostream& operator<<(std::ostream& out, const std::map<Key, Value>& container) 
{
    out << "{"s;
    Print(out, container);
    out << "}"s;

    return out;
}
 
template <typename L, typename R>
std::ostream& operator<<(std::ostream& out, const std::pair<L, R>& mypair) 
{
    const auto& [l, r] = mypair;

    out << l;
    out << ": "s;
    out << r;

    return out;
}
 
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str,
    const std::string& file, const std::string& func, unsigned line, const std::string& hint)
{
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;

        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }

        std::cerr << std::endl;

        std::abort();
    }
}
 
void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func,
    unsigned line, const std::string& hint);
 
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
 
template <typename F>
void RunTestImpl(F foo, const std::string& function_name) {
    foo();
    std::cerr << function_name << " OK" << std::endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)
   
// sprint 9   
 

// Entry point 
 