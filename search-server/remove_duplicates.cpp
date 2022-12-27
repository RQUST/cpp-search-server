#include "remove_duplicates.h"
 
using namespace std::literals;

void RemoveDuplicates(SearchServer& search_server)
{
    std::vector<int> trash(search_server.begin(), search_server.end());
    std::set<std::set<std::string>> unique_documents;

    for (const int document_id : trash) 
    {
        std::set<std::string> words_in_document;
        auto word_freq = search_server.GetWordFrequencies(document_id);

        for (auto& [word, freq] : word_freq) 
        {
            words_in_document.insert(word);
        }

        if (!unique_documents.count(words_in_document)) 
        {
            unique_documents.insert(words_in_document);
        }
        else 
        {
            std::cout << "Found duplicate document id "s << document_id << std::endl;
            search_server.RemoveDocument(document_id);
        }
    }
}