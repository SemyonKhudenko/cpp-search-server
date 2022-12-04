# cpp-search-server
---
Итоговый проект 1-3 модулей курса "Разработчик С++" на Яндекс.Практикуме -- поисковый сервер. Осуществляет поиск документов по ключевым словам с учетом их рейтинга и статуса, ранжирует результаты по релевантности. Поддерживает функциональность стоп-слов и минус-слов, поиск и удаление дубликатов. Результаты поиска выводятся постранично. Может работать в многопоточном режиме.

## Использование
---
Перед использованием потребуется изменить функцию `main` согласно вашим целям. Сейчас в `main` реализован сравнительный тест скорости работы одно- и многопоточной реализаций поиска. Основная функциональность поискового сервера реализована в конструкторах класса `SearhServer`, методах `AddDocument()` и `FindTopDocuments()`.

## Планы по доработке
---
* Реализовать CLI-интерфейс для наполнения базы данных и выполнения поисковых запросов.
* Подготовить примеры использования интерфейса поискового сервера

## Стек технологий
---
* С++17 (STL)
* CMake 3.10 или выше

# Сборка и установка
---
Проект может быть собран практически на большинстве платформ с использованием системы сборки CMake
