#include "remove_duplicates.h"

using namespace std::literals;

void RemoveDuplicates(SearchServer &search_server)
{
    std::vector<int> trash;
    std::set<std::set<std::string>> unique_documents;

    for (const int document_id : search_server)
    {
        std::set<std::string> words_in_document;
        const auto word_freq = search_server.GetWordFrequencies(document_id);

        for (auto &[word, freq] : word_freq)
        {
            words_in_document.emplace(word);
        }

        if (unique_documents.count(words_in_document))
        {
            trash.push_back(document_id);
        }
        else
        {
            unique_documents.insert(words_in_document);
        }
    }

    for (const int document_id : trash)
    {
        std::cout << "Found duplicate document id "s << document_id << std::endl;
        search_server.RemoveDocument(document_id);
    }
}