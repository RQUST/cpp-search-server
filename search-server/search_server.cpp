#include <execution> 

#include "search_server.h"
#include "string_processing.h"


SearchServer::SearchServer(const std::string& stop_words_text) :
	SearchServer(SplitIntoWords(static_cast<std::string_view>(stop_words_text))) {}

SearchServer::SearchServer(const std::string_view& stop_words_text) :
	SearchServer::SearchServer(SplitIntoWords(stop_words_text)) {}

//void SearchServer::AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings)
//{
//	if ((document_id < 0) || (documents_.count(document_id) > 0))
//	{
//		throw std::invalid_argument("Invalid document_id"s);
//	}
//
//	const auto words = SplitIntoWordsNoStop(document);
//	const double inv_word_count = 1.0 / words.size();
//
//	for (const std::string_view& word : words)
//	{
//		word_to_document_freqs_[word][document_id] += inv_word_count;
//		document_to_word_freqs_[document_id][word] += inv_word_count;
//	}
//	documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, std::string(document) });
//	document_ids_.insert(document_id);
//}

void SearchServer::AddDocument(int document_id, const std::string_view& document,
	DocumentStatus status, const std::vector<int>& ratings)
{
	if ((document_id < 0) || (documents_.count(document_id) > 0))
	{
		throw std::invalid_argument("Invalid document_id"s);
	}

	const auto words = SplitIntoWordsNoStop(document);
	const double inv_word_count = 1.0 / words.size();
	auto& word_freqs = document_to_word_freqs_[document_id];

	for (const std::string_view& word : words)
	{
		auto it = words_.insert(std::string(word));
		std::string_view sw = *it.first;

		word_to_document_freqs_[sw][document_id] += inv_word_count;
		word_freqs[sw] += inv_word_count;
	}
	documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, std::string(document) });
	document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const
{
	return FindTopDocuments(std::execution::seq, raw_query, status);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentStatus status) const
{
	return FindTopDocuments(
		std::execution::seq,
		raw_query,
		[status](int document_id, DocumentStatus document_status, int rating)
		{
			return document_status == status;
		});
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentStatus status) const
{
	return FindTopDocuments(
		std::execution::par,
		raw_query,
		[status](int document_id, DocumentStatus document_status, int rating)
		{
			return document_status == status;
		});
}


const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
	static const std::map<std::string_view, double> empty;
	static std::map<std::string_view, double> res;
	if (document_to_word_freqs_.count(document_id))
	{
		for (const auto& item : document_to_word_freqs_.at(document_id))
		{
			res.insert(item);
		};
		return res;
	}
	return empty;
}

void SearchServer::RemoveDocument(int document_id)
{
	RemoveDocument(std::execution::seq, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
	const std::string_view& raw_query, int document_id) const
{
	if (document_id < 0 || documents_.count(document_id) == 0)
	{
		throw std::out_of_range("Invalid document_id");
	}
	if (!IsValidWord(raw_query))
	{
		throw std::invalid_argument("Invalid query");
	}

	auto query = ParseQuery(raw_query, true);

	for (std::string_view word : query.minus_words)
	{
		if (word_to_document_freqs_.count(word) == 0)
		{
			continue;
		}
		if (word_to_document_freqs_.at(word).count(document_id))
		{
			return { std::vector<std::string_view>{}, documents_.at(document_id).status };
		}
	}

	std::vector<std::string_view> matched_words;
	//std::vector<std::string_view> matched_words(query.plus_words.size());

	for (std::string_view word : query.plus_words)
	{
		if (word_to_document_freqs_.count(word) == 0)
		{
			continue;
		}
		if (word_to_document_freqs_.at(word).count(document_id))
		{
			//	matched_words.push_back(*words_.find(word));
			matched_words.push_back((word));
		}
	}

	return { matched_words, documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy policy, std::string_view raw_query, int document_id) const
{
	return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
	std::execution::parallel_policy policy, std::string_view raw_query, int document_id) const
{
	if (document_id < 0 || documents_.count(document_id) == 0)
	{
		throw std::out_of_range("Invalid document_id");
	}
	if (!IsValidWord(raw_query))
	{
		throw std::invalid_argument("Invalid query");
	}

	Query query = ParseQuery(raw_query, false);

	if (any_of(
		std::execution::par,
		query.minus_words.begin(), query.minus_words.end(),
		[this, document_id](const std::string_view& word)
		{
			return word_to_document_freqs_.at(word).count(document_id);
		})) {
		return { std::vector<std::string_view>(), documents_.at(document_id).status };
	}

	std::vector<std::string_view> matched_words(query.plus_words.size());
	std::copy_if(
		std::execution::par,
		query.plus_words.begin(), query.plus_words.end(),
		matched_words.begin(),
		[this, document_id](const std::string_view& word)
		{
			return word_to_document_freqs_.at(word).count(document_id);
		});

	std::sort(std::execution::par, matched_words.begin(), matched_words.end());
	auto it = unique(std::execution::par, matched_words.begin(), matched_words.end());
	matched_words.erase(it - 1, matched_words.end());

	return { matched_words, documents_.at(document_id).status };
	//return { {matched_words.begin(), it - 1}, documents_.at(document_id).status };

	//std::vector<std::string_view> matched_words(query.plus_words.size());
	//std::vector<std::string_view> matched_words;

	/*std::sort(std::execution::par, query.plus_words.begin(), query.plus_words.end());
	auto last_plus_word = std::unique(std::execution::par, query.plus_words.begin(), query.plus_words.end());
	query.plus_words.erase(last_plus_word, query.plus_words.end());*/

	/*matched_words.resize(query.plus_words.size());

	auto it =
	std::copy_if(
		std::execution::par,
		query.plus_words.begin(), query.plus_words.end(),
		matched_words.begin(),
		[this, document_id](std::string_view word)
		{
			return word_to_document_freqs_.at(word).count(document_id);
		}
	);

	matched_words.resize(std::distance(matched_words.begin(), it));

	return { matched_words, documents_.at(document_id).status };*/
}

bool SearchServer::IsValidWord(const std::string_view& word)
{
	// A valid word must not contain special characters
	return std::none_of(word.begin(), word.end(), [](char c)
		{ return c >= '\0' && c < ' '; });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view& text) const
{
	std::vector<std::string_view> words;
	for (const std::string_view& word : SplitIntoWords(text))
	{
		if (!IsValidWord(word))
		{
			throw std::invalid_argument("Word is invalid"s);
		}
		if (!IsStopWord(word))
		{
			words.push_back(word);
		}
	}
	return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings)
{
	if (ratings.empty())
	{
		return -1;
	}

	int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
	return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const
{
	if (text.empty())
	{
		throw std::invalid_argument("Query word is empty"s);
	}

	bool is_minus = false;
	if (text[0] == '-')
	{
		is_minus = true;
		text = text.substr(1);
	}
	if (text.empty() || text[0] == '-' || !IsValidWord(text))
	{
		throw std::invalid_argument("Query word "s + static_cast<std::string>(text) + " is invalid"s);
	}
	return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, const bool b) const
{
	Query result;

	for (std::string_view word : SplitIntoWords(text))
	{
		const auto query_word = ParseQueryWord(word);
		if (!query_word.is_stop)
		{
			if (query_word.is_minus)
			{
				result.minus_words.push_back(query_word.data);
			}
			else
			{
				result.plus_words.push_back(query_word.data);
			}
		}
	}

	if (b == true)
	{
		std::sort(result.plus_words.begin(), result.plus_words.end());
		result.plus_words.erase(std::unique(result.plus_words.begin(), result.plus_words.end()),
			result.plus_words.end());

		std::sort(result.minus_words.begin(), result.minus_words.end());
		result.minus_words.erase(std::unique(result.minus_words.begin(), result.minus_words.end()),
			result.minus_words.end());

		/*sort(result.plus_words.begin(), result.plus_words.end());
		sort(result.minus_words.begin(), result.minus_words.end());

		auto last_plus_word = std::unique(result.plus_words.begin(), result.plus_words.end());
		result.plus_words.resize(std::distance(result.plus_words.begin(), last_plus_word));

		auto last_minus_word = std::unique(result.minus_words.begin(), result.minus_words.end());
		result.minus_words.resize(std::distance(result.minus_words.begin(), last_minus_word));*/

		return result;
	}

	return result;
}
