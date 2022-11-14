#include <algorithm> 
#include <map>
#include <set>
#include <string> 
#include <vector> 
#include <cmath>
#include <numeric>
#include <iostream>  
#include <utility> 

//#include "search_server.h"

using namespace std;

/* Подставьте вашу реализацию класса SearchServer сюда */
const int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double EPSILON = 1e-6;

string ReadLine() {
	string s;
	getline(cin, s);
	return s;
}
int ReadLineWithNumber() {
	int result;
	cin >> result;
	ReadLine();
	return result;
}
vector<string> SplitIntoWords(const string& text) {
	vector<string> words;
	string word;
	for (const char c : text) {
		if (c == ' ') {
			words.push_back(word);
			word = "";
		}
		else {
			word += c;
		}
	}
	words.push_back(word);
	return words;
}
struct Document {
	int id = 0;
	double relevance = 0.0;
	int rating = 0;
};
enum class DocumentStatus {
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED,
};
class SearchServer {
public:
	void SetStopWords(const string& text) {
		for (const string& word : SplitIntoWords(text)) {
			stop_words_.insert(word);
		}
	}
	void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
		const vector<string> words = SplitIntoWordsNoStop(document);
		const double inv_word_count = 1.0 / words.size();
		for (const string& word : words) {
			word_to_document_freqs_[word][document_id] += inv_word_count;
		}
		documents_.emplace(document_id,
			DocumentData{
				ComputeAverageRating(ratings),
				status
			});
	}

	template <typename DocumentPredicate>
	vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const
	{
		const Query query = ParseQuery(raw_query);
		auto matched_documents = FindAllDocuments(query, document_predicate);
		sort(matched_documents.begin(), matched_documents.end(),
			[](const Document& lhs, const Document& rhs) {
				if (abs(lhs.relevance - rhs.relevance) < 1e-6)
				{
					return lhs.rating > rhs.rating;
				}
				return lhs.relevance > rhs.relevance;
			});
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}
		return matched_documents;
	}
	/*vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status1 = DocumentStatus::ACTUAL) const
	{
		return FindTopDocuments(raw_query, [status1](int document_id, DocumentStatus status2, int rating)
			{ return  status1 == status2; });
	}*/

	vector<Document> FindTopDocuments(const string& raw_query) const
	{
		return  FindTopDocuments(raw_query, []([[maybe_unused]] int document_id,
			DocumentStatus status, int rating)
			{ return status == DocumentStatus::ACTUAL; });
	}

	vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const
	{
		auto lambda = [status](int document_id, DocumentStatus status_lambda, int rating)
		{ return status_lambda == status; };
		return  FindTopDocuments(raw_query, lambda);
	}

	size_t GetDocumentCount() const
	{
		return documents_.size();
	}
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
		return { matched_words, documents_.at(document_id).status };
	}
private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
	};
	set<string> stop_words_;
	map<string, map<int, double>> word_to_document_freqs_;
	map<int, DocumentData> documents_;
	bool IsStopWord(const string& word) const {
		return stop_words_.count(word) > 0;
	}
	vector<string> SplitIntoWordsNoStop(const string& text) const {
		vector<string> words;
		for (const string& word : SplitIntoWords(text)) {
			if (!IsStopWord(word)) {
				words.push_back(word);
			}
		}
		return words;
	}
	static int ComputeAverageRating(const vector<int>& ratings) {
		int rating_sum = 0;
		rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
		return rating_sum / static_cast<int>(ratings.size());
	}
	struct QueryWord {
		string data;
		bool is_minus;
		bool is_stop;
	};

	struct Query {
		set<string> plus_words;
		set<string> minus_words;
	};

	Query ParseQuery(const string& text) const {
		Query query;
		for (const string& word : SplitIntoWords(text)) {
			const QueryWord query_word = ParseQueryWord(word);
			if (!query_word.is_stop) {
				if (query_word.is_minus) {
					query.minus_words.insert(query_word.data);
				}
				else {
					query.plus_words.insert(query_word.data);
				}
			}
		}
		return query;
	}

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

	double ComputeWordInverseDocumentFreq(const string& word) const
	{
		return log(static_cast<double>(GetDocumentCount()) * 1.0 / static_cast<double>(word_to_document_freqs_.at(word).size()));
	}

	template<typename Predicate>
	vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const {
		map<int, double> document_to_relevance;
		for (const string& word : query.plus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
			for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
				const auto documentdata = documents_.at(document_id);
				if (predicate(document_id, documentdata.status, documentdata.rating)) {
					document_to_relevance[document_id] += term_freq * inverse_document_freq;
				}
			}
		}

		for (const string& word : query.minus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
				document_to_relevance.erase(document_id);
			}
		}

		vector<Document> matched_documents;
		for (const auto [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back(
				{ document_id, relevance, documents_.at(document_id).rating });
		}
		return matched_documents;
	}
};

/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
	const string& func, unsigned line, const string& hint)
{
	if (t != u)
	{
		cout << boolalpha;
		cout << file << "("s << line << "): "s << func << ": "s;
		cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
		cout << t << " != "s << u << "."s;
		if (!hint.empty())
		{
			cout << " Hint: "s << hint;
		}
		cout << endl;
		abort();
	}
}
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))


void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
	const string& hint)
{
	if (!value)
	{
		cout << file << "("s << line << "): "s << func << ": "s;
		cout << "ASSERT("s << expr_str << ") failed."s;
		if (!hint.empty())
		{
			cout << " Hint: "s << hint;
		}
		cout << endl;
		abort();
	}
}
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))


template <typename T>
void RunTestImpl(const T& t, const string& t_str)
{
	t();
	cerr << t_str << " OK"s;
	cerr << endl;
}

#define RUN_TEST(func)  RunTestImpl((func), #func) 

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("in"s);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
	}

	{
		SearchServer server;
		server.SetStopWords("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
	}
}

/*
Разместите код остальных тестов здесь
*/

void TestMinusWords()
{
	SearchServer server;
	int id = 1;
	vector<int> ratings = { 1, 2, 3, 5, 6, 7, 10 };
	string document1 = "зеленый крокодил длинный хвост";
	server.AddDocument(id, document1, DocumentStatus::ACTUAL, ratings);
	id = 2;
	string document2 = "зеленый попугай красный длинный хвост";
	server.AddDocument(id, document2, DocumentStatus::ACTUAL, ratings);
	id = 3;
	string document3 = "белый кот пушистый хвост";
	server.AddDocument(id, document3, DocumentStatus::ACTUAL, ratings);
	string query = "-зеленый -длинный кот хвост";
	vector<Document> found_docs = server.FindTopDocuments(query);
	ASSERT(!found_docs.empty());
	ASSERT_EQUAL(found_docs.size(), 1u);
	ASSERT_EQUAL(found_docs[0].id, 3u);
}

void TestMatching()
{
	{
		SearchServer server;
		vector<string> documents = { "белый кот пушистый хвост", "черный кот с бантом",
									"пятнистый пёс с добрыми глазами",
									"тарантул аркадий", "попугай инокентий" };

		vector<int> rating = { 1, 2, 3, 4, 5, 6, 7, 1, 1, 1, 1 };
		int i = 1;

		for (const string& doc : documents)
		{
			server.AddDocument(++i, doc, DocumentStatus::ACTUAL, rating);
		}

		vector<Document> found_doc = server.FindTopDocuments("crocodile Henrique");
		ASSERT(found_doc.empty());
	}

	{
		const int doc_id = 42;
		const string content = "cat in the city"s;
		const vector<int> ratings = { 1, 2, 3 };

		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto [words, status] = server.MatchDocument("city", 42);
		const auto doc = server.MatchDocument("city", 42);

		ASSERT(!words.empty());
		ASSERT_EQUAL(words[0], "city"s);
		ASSERT(status == DocumentStatus::ACTUAL);
	}

	{
		const int doc_id = 42;
		const string content = "cat in the city"s;
		const vector<int> ratings = { 1, 2, 3 };

		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
		const auto [words, status] = server.MatchDocument("-cat", 42);

		ASSERT(words.empty());
		ASSERT(status == DocumentStatus::BANNED);
	}

	{
		SearchServer server;

		server.AddDocument(1, "зеленый крокодил длинный хвост", DocumentStatus::ACTUAL, { 1 });
		server.AddDocument(2, "зеленый попугай красный длинный хвост", DocumentStatus::ACTUAL, { 2 });
		server.AddDocument(3, "белый кот пушистый хвост", DocumentStatus::ACTUAL, { 3 });
		server.AddDocument(4, "пушистый кот", DocumentStatus::ACTUAL, { 4 });
		server.AddDocument(5, "пушистый кот", DocumentStatus::ACTUAL, { 1, 3, -7 });

		vector<Document> doc = server.FindTopDocuments("белый пушистый кот");

		ASSERT(doc[0].rating == 3);
		ASSERT(doc[1].rating == 4);
		ASSERT(doc[2].rating == -1);

		ASSERT(doc[0].relevance > doc[1].relevance);
		ASSERT(doc[0].relevance > doc[2].relevance);

		const int doc_id = 5;
		vector<Document> doc2 = server.FindTopDocuments("белый пушистый кот",
			[](int id, DocumentStatus, int raiting) {return id > 5; });

		ASSERT(doc2.empty());
	}
}

void TestRelevance()
{
	SearchServer server;
	int id = 1;
	vector<int> ratings_1 = { 1 };
	DocumentStatus status = DocumentStatus::ACTUAL;
	string document1 = "зеленый крокодил длинный хвост";
	server.AddDocument(id, document1, status, ratings_1);

	id = 2;
	vector<int> ratings_2 = { 2 };
	string document2 = "зеленый попугай красный длинный хвост";
	server.AddDocument(id, document2, status, ratings_2);

	id = 3;
	vector<int> ratings_3 = { 3 };
	string document3 = "белый кот пушистый хвост";
	server.AddDocument(id, document3, status, ratings_3);

	id = 4;
	vector<int> ratings_4 = { 4 };
	string document4 = "пушистый кот";
	server.AddDocument(id, document4, status, ratings_4);

	vector<Document> doc = server.FindTopDocuments("белый пушистый кот");

	ASSERT_EQUAL(doc.size(), 2u);
	ASSERT_EQUAL(doc[0].id, 4);
	ASSERT_EQUAL(doc[1].id, 3);

}

void TestRating()
{
	SearchServer server;
	int id = 1;
	vector<int> ratings_1 = { 1, 1, 1 };
	DocumentStatus status = DocumentStatus::ACTUAL;

	string document1 = "зеленый крокодил длинный хвост";
	server.AddDocument(id, document1, status, ratings_1);

	id = 2;
	vector<int> ratings_2 = { 2, 2, 2 };
	string document2 = "зеленый попугай красный длинный хвост";
	server.AddDocument(id, document2, status, ratings_2);

	id = 3;
	vector<int> ratings_3 = { 3, 3, 3 };
	string document3 = "белый кот пушистый хвост";
	server.AddDocument(id, document3, status, ratings_3);

	id = 4;
	vector<int> ratings_4 = { 4, 4, 4 };
	string document4 = "пушистый кот";
	server.AddDocument(id, document4, status, ratings_4);

	string query = "кот хвост";

	vector<Document> found_docs_rating_1 =
		server.FindTopDocuments(query, [](int document_id, DocumentStatus status,
			int rating) { return rating == 1; });
	ASSERT_EQUAL(found_docs_rating_1.size(), 1u);
	ASSERT_EQUAL(found_docs_rating_1.at(0).id, 1);

	vector<Document> found_docs_rating_2 =
		server.FindTopDocuments(query, [](int document_id, DocumentStatus status,
			int rating) { return rating == 2; });
	ASSERT_EQUAL(found_docs_rating_2.size(), 1u);
	ASSERT_EQUAL(found_docs_rating_2.at(0).id, 2);

	vector<Document> found_docs_rating_3 =
		server.FindTopDocuments(query, [](int document_id, DocumentStatus status,
			int rating) { return rating == 3; });
	ASSERT_EQUAL(found_docs_rating_3.size(), 1u);
	ASSERT_EQUAL(found_docs_rating_3.at(0).id, 3);

	vector<Document> found_docs_rating_4 =
		server.FindTopDocuments(query, [](int document_id, DocumentStatus status,
			int rating) { return rating == 4; });
	ASSERT_EQUAL(found_docs_rating_4.size(), 1u);
	ASSERT_EQUAL(found_docs_rating_4.at(0).id, 4);
}

void testPredicat()
{
	SearchServer server;
	server.AddDocument(1, "kot boris with furry tail"s, DocumentStatus::REMOVED,
		{ 5 });
	server.AddDocument(2, "kot Henry with kind eyes"s, DocumentStatus::ACTUAL,
		{ 6 });
	vector<Document> found_doc = server.FindTopDocuments(
		"kot", [](int document_id, DocumentStatus status, int rating)
		{ return status == DocumentStatus::REMOVED; });

	ASSERT_EQUAL(found_doc.size(), 1);
	ASSERT_EQUAL(found_doc[0].id, 1);
}

void TestStatus()
{
	SearchServer server;
	int id = 1;
	vector<int> ratings = { 1, 2, 3, 5, 6, 7, 10 };
	string document1 = "зеленый крокодил длинный хвост";
	server.AddDocument(id, document1, DocumentStatus::BANNED, ratings);

	id = 2;
	string document2 = "зеленый попугай красный длинный хвост";
	server.AddDocument(id, document2, DocumentStatus::IRRELEVANT, ratings);

	id = 3;
	string document3 = "белый кот пушистый хвост";
	server.AddDocument(id, document3, DocumentStatus::REMOVED, ratings);

	id = 4;
	string document4 = "пушистый кот";
	server.AddDocument(id, document4, DocumentStatus::ACTUAL, ratings);

	string query = "кот хвост";

	vector<Document> found_docs_banned =
		server.FindTopDocuments(query, DocumentStatus::BANNED);
	ASSERT_EQUAL(found_docs_banned.size(), 1u);
	ASSERT_EQUAL(found_docs_banned.at(0).id, 1);

	vector<Document> found_docs_irrelevant =
		server.FindTopDocuments(query, DocumentStatus::IRRELEVANT);
	ASSERT_EQUAL(found_docs_irrelevant.size(), 1u);
	ASSERT_EQUAL(found_docs_irrelevant.at(0).id, 2);

	vector<Document> found_docs_removed =
		server.FindTopDocuments(query, DocumentStatus::REMOVED);
	ASSERT_EQUAL(found_docs_removed.size(), 1u);
	ASSERT_EQUAL(found_docs_removed.at(0).id, 3);

	vector<Document> found_docs_actual =
		server.FindTopDocuments(query, DocumentStatus::ACTUAL);
	ASSERT_EQUAL(found_docs_actual.size(), 1u);
	ASSERT_EQUAL(found_docs_actual.at(0).id, 4);
}

void TestFindByRelevance()
{
	SearchServer server;
	int id = 1;
	vector<int> ratings_1 = { 1 };
	DocumentStatus status = DocumentStatus::ACTUAL;
	string document1 = "зеленый крокодил длинный хвост";
	server.AddDocument(id, document1, status, ratings_1);

	id = 2;
	vector<int> ratings_2 = { 2 };
	string document2 = "зеленый попугай красный длинный хвост";
	server.AddDocument(id, document2, status, ratings_2);

	id = 3;
	vector<int> ratings_3 = { 3 };
	string document3 = "белый кот пушистый хвост";
	server.AddDocument(id, document3, status, ratings_3);

	id = 4;
	vector<int> ratings_4 = { 4 };
	string document4 = "пушистый кот";
	server.AddDocument(id, document4, status, ratings_4);

	string query = "белый пушистый кот длинный хвост";

	vector<Document> found_doc_1 = server.FindTopDocuments(query);
	ASSERT(!found_doc_1.empty());
	ASSERT_EQUAL(found_doc_1[0].id, 3);

	vector<Document> found_doc_2 = server.FindTopDocuments(query);
	ASSERT(!found_doc_2.empty());
	ASSERT_EQUAL(found_doc_2[1].id, 4);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer()
{
	RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
	// Не забудьте вызывать остальные тесты здесь
	RUN_TEST(TestMinusWords);
	RUN_TEST(TestMatching);
	RUN_TEST(TestRelevance);
	RUN_TEST(TestRating);
	RUN_TEST(testPredicat);
	RUN_TEST(TestStatus);
	RUN_TEST(TestFindByRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
	TestSearchServer();
	// Если вы видите эту строку, значит все тесты прошли успешно
	cout << "Search server testing finished"s << endl;
}
