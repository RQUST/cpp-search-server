# Поисковая система SearchServer

## Описание
SearchServer - система поиска документов по ключевым словам.

Основной функционал: 
- ранжирование результатов поиска по статистической мере TF-IDF;
- обработка стоп-слов (не учитываются поисковой системой и не влияют на результаты поиска);
- обработка минус-слов (документы, содержащие минус-слова, не будут включены в результаты поиска);
- создание и обработка очереди запросов;
- удаление дубликатов документов;
- постраничное разделение результатов поиска;
- возможность работы в многопоточном режиме.

## Системные требования
Компилятор с поддержкой стандарта C++17 или выше
Библиотека Thread Building Blocks от Intel (libtbb-dev)