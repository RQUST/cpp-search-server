#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <map>
#include <set>
#include <string>
#include <optional>
#include <utility>
#include <vector>

using namespace std;

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
			if (!word.empty()) {
				words.push_back(word);
				word.clear();
			}
		}
		else {
			word += c;
		}
	}
	if (!word.empty()) {
		words.push_back(word);
	}

	return words;
}

struct Document {
	Document() = default;

	Document(int id, double relevance, int rating)
		: id(id)
		, relevance(relevance)
		, rating(rating)
	{
	}

	int id = 0;
	double relevance = 0.0;
	int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings)
{
	set<string> non_empty_strings;
	for (const string& str : strings) {
		if (!str.empty()) {
			non_empty_strings.insert(str);
		}
	}
	return non_empty_strings;
}

enum class DocumentStatus {
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED,
};

class SearchServer
{
public:
	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words)
		: stop_words_(MakeUniqueNonEmptyStrings(stop_words))
	{
		for (const auto s : stop_words_)
		{
			if (!CheckSpecialSymbols(s))
			{
				throw invalid_argument(s);
			}
		}
	}

	explicit SearchServer(const string& stop_words_text)
		: SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
	{
	}

	void  AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings)
	{
		if (document_id < 0)
		{
			throw invalid_argument("AddDocument document_id < 0 "s);
		}
		if (documents_.count(document_id))
		{
			string s = "AddDocument The ID "s + to_string(document_id) + "already exists "s;
			throw invalid_argument(s);
		}
		ids_.push_back(document_id);

		const vector<string> words = SplitIntoWordsNoStop(document);
		const double inv_word_count = 1.0 / words.size();
		for (const string& word : words)
		{
			word_to_document_freqs_[word][document_id] += inv_word_count;
		}
		documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
	}

	template <typename DocumentPredicate>
	vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const
	{
		const Query query = ParseQuery(raw_query);
		auto matched_documents = FindAllDocuments(query, document_predicate);

		sort(matched_documents.begin(), matched_documents.end(),
			[](const Document& lhs, const Document& rhs) {
				if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
					return lhs.rating > rhs.rating;
				}
				else {
					return lhs.relevance > rhs.relevance;
				}
			});
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}

		return matched_documents;
	}

	vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const
	{
		return FindTopDocuments
		(raw_query, [status](int document_id, DocumentStatus document_status, int rating)
			{  return document_status == status; });
	}

	vector<Document> FindTopDocuments(const string& raw_query) const
	{
		return FindTopDocuments
		(raw_query, [](int document_id, DocumentStatus document_status, int rating)
			{  return document_status == DocumentStatus::ACTUAL; });
	}

	size_t GetDocumentCount() const
	{
		return documents_.size();
	}

	tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const
	{
		const Query query = ParseQuery(raw_query);
		vector<string> matched_words;

		for (const string& word : query.plus_words)
		{
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			if (word_to_document_freqs_.at(word).count(document_id)) {
				matched_words.push_back(word);
			}
		}

		for (const string& word : query.minus_words)
		{
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			if (word_to_document_freqs_.at(word).count(document_id))
			{
				matched_words.clear();
				break;
			}
		}

		return std::tie(matched_words, documents_.at(document_id).status);
	}

	int GetDocumentId(int index) const
	{
		if (index < 0 || index >= documents_.size())
		{
			throw out_of_range("GetDocumentId out_of_range"s);
		}

		return ids_[index];
	}

private:
	static bool IsValidWord(const string& word)
	{
		// A valid word must not contain special characters
		return none_of(word.begin(), word.end(), [](char c) {
			return c >= '\0' && c < ' ';
			});
	}

	[[nodiscard]] bool CheckSpecialSymbols(const string& in) const
	{
		unsigned int let_num = 0;
		for (const char letter : in)
		{
			let_num = static_cast<unsigned int>(letter);
			if (let_num < 32) return false;
		}
		return true;
	}

	[[nodiscard]] bool CheckDoubleMinus(const string& in) const
	{
		bool prev_minus = false;
		for (const char letter : in)
		{
			if (letter == '-')
			{
				if (prev_minus == true)
				{
					return false;
				}
				else prev_minus = true;
			}
			else prev_minus = false;
		}

		return true;
	}

	[[nodiscard]] bool CheckNoMinusWord(const string& in) const
	{
		const unsigned int j = in.size();
		if (in[j - 1] == '-') return false;

		for (unsigned int i = 0; i < j; ++i)
		{
			if (in[i] == '-') if (in[i + 1] == ' ') return false;
		}

		return true;
	}

	struct DocumentData {
		int rating;
		DocumentStatus status;
	};
	const set<string> stop_words_;
	map<string, map<int, double>> word_to_document_freqs_;
	map<int, DocumentData> documents_;
	vector<int> ids_;

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
		if (ratings.empty()) {
			return 0;
		}
		int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
		return rating_sum / static_cast<int>(ratings.size());
	}

	struct QueryWord
	{
		string data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(const string& text) const
	{  
		if (!IsValidWord(text))
		{
			throw invalid_argument(std::string("invalid characters in query: ") + text);
		}

		string str = text;

		bool is_minus = false;
		// Word shouldn't be empty

		if (text[0] == '-')
		{
			if (str.length() == 0)
			{
				throw invalid_argument("iblank negative request: "s);
			}
			if (str[1] == '-')
			{
				throw invalid_argument(std::string("double minus: ") + text);
			}
			is_minus = true;
			str = text.substr(1);
		}
		return { str, is_minus, IsStopWord(str) };
	}

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

	double ComputeWordInverseDocumentFreq(const string& word) const {
		return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
	}

	template <typename DocumentPredicate>
	vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const
	{
		map<int, double> document_to_relevance;
		for (const string& word : query.plus_words)
		{
			if (word_to_document_freqs_.count(word) == 0)
			{
				continue;
			}
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

			for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
			{
				const DocumentData& document_data = documents_.at(document_id);
				if (document_predicate(document_id, document_data.status, document_data.rating))
				{
					document_to_relevance[document_id] += term_freq * inverse_document_freq;
				}
			}
		}

		for (const string& word : query.minus_words)
		{
			if (word_to_document_freqs_.count(word) == 0)
			{
				continue;
			}
			for (const auto [document_id, _] : word_to_document_freqs_.at(word))
			{
				document_to_relevance.erase(document_id);
			}
		}

		vector<Document> matched_documents;
		for (const auto [document_id, relevance] : document_to_relevance)
		{
			matched_documents.push_back(
				{ document_id, relevance, documents_.at(document_id).rating });
		}
		return matched_documents;
	}
};

void PrintDocument(const Document& document) {
	cout << "{ "s
		<< "document_id = "s << document.id << ", "s
		<< "relevance = "s << document.relevance << ", "s
		<< "rating = "s << document.rating << " }"s << endl;
}

int main()
{
	setlocale(LC_ALL, "");

	try
	{
		SearchServer search_server("����\x12���"s);

		// ���� ���������� ��������� ������ AddDocument, ����� �������� ��������������
		// � �������������� ���������� ��� ������
		(void)search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
		search_server.AddDocument(1, "�������� �� � ������ �������"s, DocumentStatus::ACTUAL, { 1, 2 });


		const auto documents = search_server.FindTopDocuments("--��������"s);

		for (const Document& document : documents)
		{
			PrintDocument(document);
		}
	}
	catch (const std::invalid_argument& e)
	{
		cout << "invalid_argument: "s << e.what() << endl;
	}

	try
	{
		SearchServer search_server("� � ��"s);

		// ���� ���������� ��������� ������ AddDocument, ����� �������� ��������������
		// � �������������� ���������� ��� ������
		(void)search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
		search_server.AddDocument(1, "�������� �� � ������ �������"s, DocumentStatus::ACTUAL, { 1, 2 });


		const auto documents = search_server.FindTopDocuments("--��������"s);

		for (const Document& document : documents)
		{
			PrintDocument(document);
		}
	}
	catch (const std::invalid_argument& e)
	{
		cout << "invalid_argument: "s << e.what() << endl;
	}

	try
	{
		SearchServer search_server("� � ��"s);

		(void)search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });

		search_server.AddDocument(-1, "�������� �� � ������ �������"s, DocumentStatus::ACTUAL, { 1, 2 });
	}
	catch (const std::invalid_argument& e)
	{
		cout << "invalid_argument: "s << e.what() << endl;
	}

	try
	{
		SearchServer search_server("� � ��"s);

		(void)search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });

		search_server.AddDocument(3, "������� �� ����\x12���"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
	}
	catch (const std::invalid_argument& e)
	{
		cout << "invalid_argument: "s << e.what() << endl;
	}

	try
	{
		SearchServer search_server("� � ��"s);

		// ���� ���������� ��������� ������ AddDocument, ����� �������� ��������������
		// � �������������� ���������� ��� ������
		(void)search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });


		const auto documents = search_server.FindTopDocuments("����\x12���"s);

		for (const Document& document : documents)
		{
			PrintDocument(document);
		}
	}
	catch (const std::invalid_argument& e)
	{
		cout << "invalid_argument: "s << e.what() << endl;
	}

	try
	{
		SearchServer search_server("� � ��"s);

		// ���� ���������� ��������� ������ AddDocument, ����� �������� ��������������
		// � �������������� ���������� ��� ������
		(void)search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });


		const auto documents = search_server.FindTopDocuments("--��������"s);

		for (const Document& document : documents)
		{
			PrintDocument(document);
		}
	}
	catch (const std::invalid_argument& e)
	{
		cout << "invalid_argument: "s << e.what() << endl;
	}

	try
	{
		SearchServer search_server("� � ��"s);

		// ���� ���������� ��������� ������ AddDocument, ����� �������� ��������������
		// � �������������� ���������� ��� ������
		(void)search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });


		const auto documents = search_server.FindTopDocuments("�������� - "s);

		for (const Document& document : documents)
		{
			PrintDocument(document);
		}
	}
	catch (const std::invalid_argument& e)
	{
		cout << "invalid_argument: "s << e.what() << endl;
	}

	try
	{
		SearchServer search_server("� � ��"s);

		// ���� ���������� ��������� ������ AddDocument, ����� �������� ��������������
		// � �������������� ���������� ��� ������
		(void)search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, { 7, 2, 7 });


	}
	catch (const std::invalid_argument& e)
	{
		cout << "invalid_argument: "s << e.what() << endl;
	}
}
