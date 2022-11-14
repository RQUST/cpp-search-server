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
		server.AddDocument(5, "пушистый кот", DocumentStatus::ACTUAL, { 4 });

		vector<Document> doc = server.FindTopDocuments("белый пушистый кот");

		ASSERT(doc[0].rating == 3);
		ASSERT(doc[1].rating == 4);
		ASSERT(doc[2].rating == 4);

		ASSERT(doc[0].relevance > doc[1].relevance);
		ASSERT(doc[0].relevance > doc[2].relevance);

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
 