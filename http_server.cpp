#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <sqlite3.h>

using namespace std;
using namespace boost::asio;
using ip::tcp;

unordered_map<string, string> code_errors = {
    {"200", "HTTP/1.1 200 OK\r\n"},
    {"404", "HTTP/1.1 404 Not Found\r\n"},
    {"405", "HTTP/1.1 405 Method Not Allowed\r\n"},
    {"301", "HTTP/1.1 301 Moved Permanently\r\n"},
    {"500", "HTTP/1.1 500 Internal Server Error\r\n"},
    {"505", "HTTP/1.1 505 HTTP Version Not Supported\r\n"},
    {"418", "HTTP/1.1 418 I'm a teapot\r\n"}
};

void submitPet(unordered_map<string, string> params) {
    string name = params["name"];
    string type = params["type"];
    string owner = params["owner"];

    sqlite3 *db;
    sqlite3_open("pets.db", &db);
    string sql = "INSERT INTO pets (name, type, owner) VALUES ('" + name + "', '" + type + "', '" + owner + "');";
    sqlite3_exec(db, sql.c_str(), 0, 0, 0);
    sqlite3_close(db);
}

string searchPet(const string& name) {
    //cout<<name<<endl;
    sqlite3 *db;
    sqlite3_stmt *stmt;
    string data = "";

    sqlite3_open("pets.db", &db);

    string sql = "SELECT * FROM pets WHERE name = ?;";
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        data += "ID: " + to_string(sqlite3_column_int(stmt, 0)) + "\n";
        data += "Name: " + string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))) + "\n";
        data += "Type: " + string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) + "\n";
        data += "Owner: " + string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))) + "\n";
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return data;
}

string header(const string& code, size_t size = 0, const string& type = "", const string& location = "") {
    string response = code_errors[code];
    if (code == "301") {
        response += "Location: http://localhost:8000/" + location + "\r\n";
    }
    if (!type.empty()) {
        response += "Content-Type: " + type + "\r\n";
    }
    if (size > 0) {
        response += "Content-Length: " + to_string(size) + "\r\n";
    }
    response += "\r\n";
    return response;
}

string read_file(const string& filename) {
    ifstream file(filename, ios::binary);
    if (file) {
        ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }
    return "";
}

string get_content_type(const string& filename) {
    if (boost::algorithm::iends_with(filename, ".html")) {
        return "text/html";
    } else if (boost::algorithm::iends_with(filename, ".jpg") || boost::algorithm::iends_with(filename, ".jpeg")) {
        return "image/jpeg";
    } else if (boost::algorithm::iends_with(filename, ".png")) {
        return "image/png";
    } else {
        return "application/octet-stream";
    }
}

unordered_map<string, string> parse_GET_values(const string& request) {
    size_t positionOfGETValues = request.find("?");
    string GETValues = request.substr(positionOfGETValues + 1);
    unordered_map<string, string> params;
    istringstream query_stream(GETValues);
    string key_value;
    while (getline(query_stream, key_value, '&')) {
        auto pos = key_value.find('=');
        if (pos != string::npos) {
            string key = key_value.substr(0, pos);
            string value = key_value.substr(pos + 1);
            params[key] = value;
        }
    }
    return params;
}
string generatePetHTML(const string& petData) {
    istringstream iss(petData);
    string id, name, type, owner;
    getline(iss, id);
    getline(iss, name);
    getline(iss, type);
    getline(iss, owner);

    id = id.substr(id.find(":") + 2);
    name = name.substr(name.find(":") + 2);
    type = type.substr(type.find(":") + 2);
    owner = owner.substr(owner.find(":") + 2);
    

    string html = "<!DOCTYPE html>\n"
                  "<html>\n"
                  "<head>\n"
                  "<title>Pet Search</title>\n"
                  "</head>\n"
                  "<body>\n"
                  "<h1>Pet Data</h1>\n"
                  "<ul>\n"
                  "<li>Name: " + name + "</li>\n"
                  "<li>Type: " + type + "</li>\n"
                  "<li>Owner: " + owner + "</li>\n"
                  "</ul>\n"
                  "</body>\n"
                  "</html>";
    return html;
}
string handle_request(const string& request) {
    try {
        istringstream iss(request);
        string method, path, version;
        iss >> method >> path >> version;

        if (version.substr(0, 8) != "HTTP/1.1") {
            return header("505");
        }

        if (method != "GET") {
            if (method == "BREW") {
                string body = read_file("teapot.html");
                string content_type = get_content_type("teapot.html");
                return header("418", body.size(), content_type) + body;
            } else {
                return header("405");
            }
        }

        size_t positionOfStartPath = request.find(" ");
        string pathWithoutGETValues = request.substr(positionOfStartPath + 1, request.find("?") - positionOfStartPath - 1);

        if (pathWithoutGETValues == "/submitPet"){
            submitPet(parse_GET_values(path));
            return header("200", 0, "text/plain") + "Pet submitted successfully!";
        }
        if (pathWithoutGETValues == "/searchPet"){
            unordered_map<string, string> petDataMap = parse_GET_values(path);
            string petName = "";
            if (petDataMap.find("name") != petDataMap.end()) {
                petName = petDataMap["name"];
            } else {
                // Handle the case where "name" is not in petDataMap
                // For example, you can return an error message
                string errorMessage = "Pet name not provided";
                return header("400", errorMessage.size(), "text/plain") + errorMessage;
            }
            string petData = searchPet(petName);
            string petHTML = generatePetHTML(petData);
            return header("200", petHTML.size(), "text/html") + petHTML;
        }
        if (path == "/") {
            path = "/index.html";
        }

        string filename = path.substr(1);
        if (boost::filesystem::exists(filename) && boost::filesystem::is_regular_file(filename)) {
            string body = read_file(filename);
            string content_type = get_content_type(filename);
            return header("200", body.size(), content_type) + body;
        } else if (boost::filesystem::exists(filename + "/index.html")) {
            string body = read_file(filename + "/index.html");
            string content_type = get_content_type(filename + "/index.html");
            return header("200", body.size(), content_type) + body;
        } else if (boost::filesystem::exists(filename + "index.html")) {
            return header("301", 0, "", filename + "index.html");
        } else {
            return header("404");
        }
    } catch (...) {
        return header("500");
    }
}

void run_server() {
    io_service io_service;
    tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), 8000));

    while (true) {
        tcp::socket socket(io_service);
        acceptor.accept(socket);

        char data[1024];
        boost::system::error_code error;
        size_t length = socket.read_some(buffer(data), error);
        if (!error) {
            string request(data, length);
            string response = handle_request(request);
            write(socket, buffer(response), error);
        }

        socket.close();
    }
}

int main() {
    try {
        sqlite3* db;
        sqlite3_open("pets.db", &db);
        const char* sql = "CREATE TABLE IF NOT EXISTS pets (id INTEGER PRIMARY KEY, name TEXT, type TEXT, owner TEXT);";
        sqlite3_exec(db, sql, 0, 0, 0);
        sqlite3_close(db);
        run_server();
    } catch (exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }
    return 0;
}