#pragma once

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <stdexcept> 
#include <utility>
#include <vector>
#include <numeric>
#include <string> 
#include <string_view>
#include <iterator>
#include <execution>


#include "concurrent_map.h"
#include "read_input_functions.h"
#include "string_processing.h"
#include "document.h"

// #include "tbb/blocked_range.h"

using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double EPSILON = 1e-6;


class SearchServer
{
public:
	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words);

	explicit SearchServer(const std::string& stop_words_text);
	explicit SearchServer(const std::string_view& stop_words_text);

	void AddDocument(int document_id, const std::string_view& document, DocumentStatus status,
		const std::vector<int>& ratings);


	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
		return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
	}

	template <typename DocumentPredicate, typename ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy,
		std::string_view raw_query, DocumentPredicate document_predicate) const;

	std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;

	std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&,
		std::string_view raw_query, DocumentStatus status) const;

	std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&,
		std::string_view raw_query, DocumentStatus status) const;


	std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const {
		return FindTopDocuments(std::execution::seq, raw_query);
	}
	std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query) const {
		return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
	}
	std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query) const {
		return FindTopDocuments(std::execution::par, raw_query, DocumentStatus::ACTUAL);
	}

	int GetDocumentCount() const { return documents_.size(); }

	int GetDocumentId(int index) const { return document_ids_.count(index); }

	std::set<int>::iterator begin() const { return document_ids_.begin(); }
	std::set<int>::iterator end() const { return document_ids_.end(); }

	std::set<int>::const_iterator cbegin() const { return document_ids_.begin(); }
	std::set<int>::const_iterator cend() const { return document_ids_.end(); }

	const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

	void RemoveDocument(int document_id);

	template<class ExecutionPolicy>
	void RemoveDocument(ExecutionPolicy&& policy, int document_id);

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
		const std::string_view& raw_query, int document_id) const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
		std::execution::sequenced_policy policy, std::string_view raw_query, int document_id) const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
		std::execution::parallel_policy policy, std::string_view raw_query, int document_id) const;

private:
	struct DocumentData
	{
		int rating;
		DocumentStatus status;
		std::string content;
	};

	std::set<std::string, std::less<>> words_;

	const std::set<std::string, std::less<>> stop_words_;
	std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
	std::map<int, std::map<std::string_view, double>, std::less<>> document_to_word_freqs_;
	std::map<int, DocumentData> documents_;
	std::set<int> document_ids_;

	bool IsStopWord(const std::string_view word) const
	{
		return stop_words_.count(word) > 0;
	}

	static bool IsValidWord(const std::string_view& word);

	std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view& text) const;

	static int ComputeAverageRating(const std::vector<int>& ratings);

	struct QueryWord
	{
		std::string_view data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(std::string_view text) const;

	struct Query
	{
		std::vector<std::string_view> plus_words;
		std::vector<std::string_view> minus_words;
	};

	Query ParseQuery(std::string_view text, const bool b) const;

	// Existence required
	double ComputeWordInverseDocumentFreq(const std::string_view& word) const {
		return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
	}

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
		return FindAllDocuments(std::execution::seq, query, document_predicate);
	}

	template <typename DocumentPredicate, typename ExecutionPolicy>
	std::vector<Document> FindAllDocuments(ExecutionPolicy&& policy, const Query& query,
		DocumentPredicate document_predicate) const;
};

template<typename StringContainer>
inline SearchServer::SearchServer(const StringContainer& stop_words) :
	stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
	if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
		throw std::invalid_argument("Some of stop words are invalid"s);
	}
}

template<typename DocumentPredicate, typename ExecutionPolicy>
inline std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentPredicate document_predicate) const
{
	const auto query = ParseQuery(raw_query, true);

	auto matched_documents = FindAllDocuments(policy, query, document_predicate);

	std::sort(
		policy,
		matched_documents.begin(), matched_documents.end(),
		[](const Document& lhs, const Document& rhs) {
			return lhs.relevance > rhs.relevance
			|| (std::abs(lhs.relevance - rhs.relevance) < EPSILON && lhs.rating > rhs.rating);
		});

	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;
}

template<class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id)
{
	if (!document_ids_.count(document_id))
	{
		return;
	}

	const auto& word_freqs = document_to_word_freqs_.at(document_id);
	std::vector<const std::string_view*> words(word_freqs.size());

	transform(
		word_freqs.begin(), word_freqs.end(),
		words.begin(),
		[](const auto& i)
		{	return &i.first; });

	for_each(
		policy,
		words.begin(), words.end(),
		[this, document_id](const auto* ptr)
		{ word_to_document_freqs_.at(*ptr).erase(document_id); });

	documents_.erase(document_id);
	document_to_word_freqs_.erase(document_id);
	document_ids_.erase(find(document_ids_.begin(), document_ids_.end(), document_id));
}

template<typename DocumentPredicate, typename ExecutionPolicy>
inline std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, const Query& query, DocumentPredicate document_predicate) const
{
	ConcurrentMap<int, double> cm_document_to_relevance(100);

	std::for_each(
		policy,
		query.plus_words.begin(), query.plus_words.end(),
		[this, &document_predicate, &cm_document_to_relevance](std::string_view word)
		{
			if (word_to_document_freqs_.count(word) == 0)
			{
				return;
			}

	const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
	for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word))
	{

		const auto& document_data = documents_.at(document_id);
		if (document_predicate(document_id, document_data.status, document_data.rating))
		{
			cm_document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
		}
	}
		}
	);

	std::map<int, double> document_to_relevance(cm_document_to_relevance.BuildOrdinaryMap());

	for (const std::string_view& word : query.minus_words)
	{
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
			document_to_relevance.erase(document_id);
		}
	}

	std::vector<Document> matched_documents;
	for (const auto& [document_id, relevance] : document_to_relevance)
	{
		matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
	}
	return matched_documents;
} 
