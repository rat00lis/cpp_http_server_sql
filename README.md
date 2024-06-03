# cpp_http_server_sql

Simple http server with form to add pets to a database (SQLite).

To run, first compile:

`g++ -o server http_server.cpp -lboost_system -lboost_filesystem -lsqlite3 -pthread`

then run:

`./server`