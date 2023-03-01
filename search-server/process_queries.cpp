#include <execution>
#include <algorithm>
#include <list>

#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries)
{
	std::vector<std::vector<Document>> process_queries(queries.size());

	std::transform(
		std::execution::par,
		queries.begin(), queries.end(),
		process_queries.begin(),
		[&search_server](const auto& query)
		{ return search_server.FindTopDocuments(query); });

	return process_queries;

}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries)
{
	std::list<Document> joined_documents;

	for (const auto& documents : ProcessQueries(search_server, queries))
	{  
		// 1
		/*std::transform(
			std::make_move_iterator(documents.begin()), std::make_move_iterator(documents.end()),
			std::back_inserter(joined_documents),
			[](auto& document) { return std::move(document);
			});
		*/

		// 2
		joined_documents.insert(joined_documents.end(), documents.begin(), documents.end());

		// 3
		/*for (auto& it : documents)
		{
			joined_documents.push_back(std::move(it));
		}*/
	}

	return joined_documents;
}
