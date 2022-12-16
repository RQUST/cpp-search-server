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


#include "read_input_functions.h"
#include "string_processing.h"
#include "document.h"

using namespace std::string_literals; 

const int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double EPSILON = 1e-6;


class SearchServer
{
public:
	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words);

	explicit SearchServer(const std::string& stop_words_text);

	void AddDocument(int document_id, const std::string& document, DocumentStatus status,
		const std::vector<int>& ratings);

	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(const std::string& raw_query,
		DocumentPredicate document_predicate) const;


	std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;

	std::vector<Document> FindTopDocuments(const std::string& raw_query) const {
		return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
	}

	size_t GetDocumentCount() const { return documents_.size(); }

	int GetDocumentId(int index) const { return document_ids_.at(index); }

	std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query,
		int document_id) const;

private:
	struct DocumentData
	{
		int rating;
		DocumentStatus status;
	};

	const std::set<std::string> stop_words_;
	std::map<std::string, std::map<int, double>> word_to_document_freqs_;
	std::map<int, DocumentData> documents_;
	std::vector<int> document_ids_;
	bool IsStopWord(const std::string& word) const {
		return stop_words_.count(word) > 0;
	}

	static bool IsValidWord(const std::string& word);

	std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

	static int ComputeAverageRating(const std::vector<int>& ratings);

	struct QueryWord
	{
		std::string data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(const std::string& text) const;

	struct Query
	{
		std::set<std::string> plus_words;
		std::set<std::string> minus_words;
	};

	Query ParseQuery(const std::string& text) const;

	// Existence required
	double ComputeWordInverseDocumentFreq(const std::string& word) const{
		return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());}

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const Query& query,
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

template<typename DocumentPredicate>
inline std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const
{
	const auto query = ParseQuery(raw_query);

	auto matched_documents = FindAllDocuments(query, document_predicate);

	std::sort(matched_documents.begin(), matched_documents.end(),
		[](const Document& lhs, const Document& rhs) {
			return lhs.relevance > rhs.relevance
			|| (std::abs(lhs.relevance - rhs.relevance) < EPSILON && lhs.rating > rhs.rating);
		});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;
}

template<typename DocumentPredicate>
inline std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const
{
	std::map<int, double> document_to_relevance;
	for (const std::string& word : query.plus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
		for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
			const auto& document_data = documents_.at(document_id);
			if (document_predicate(document_id, document_data.status, document_data.rating)) {
				document_to_relevance[document_id] += term_freq * inverse_document_freq;
			}
		}
	}
	for (const std::string& word : query.minus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
			document_to_relevance.erase(document_id);
		}
	}

	std::vector<Document> matched_documents;
	for (const auto [document_id, relevance] : document_to_relevance) {
		matched_documents.push_back(
			{ document_id, relevance, documents_.at(document_id).rating });
	}
	return matched_documents;
}
