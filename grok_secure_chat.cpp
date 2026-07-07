#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string sanitize(const std::string& in) {
    std::string s = in.substr(0, 4000);
    std::string out;
    for (unsigned char c : s) {
        if (std::iscntrl(c) && c != '\t' && c != '\n') continue;
        out += c;
    }
    const std::vector<std::string> bad = {";","|","&","`","$","eval(","exec(","rm ","../","<script"};
    for (auto& p : bad) if (out.find(p) != std::string::npos) return "[BLOCKED]";
    return out;
}

std::string get_api_key() {
    const char* env = std::getenv("XAI_API_KEY");
    if (env && *env) return env;

    std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
    std::string keyfile = home + "/.config/xai/api_key";

    std::ifstream f(keyfile);
    if (f.is_open()) {
        std::string k;
        std::getline(f, k);
        if (!k.empty()) return k;
    }
    return "";
}

std::string extract_content(const std::string& json) {
    size_t pos = json.find("\"content\":\"");
    if (pos == std::string::npos) return "[No content in response]";
    pos += 11;
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "[Parse error]";
    return json.substr(pos, end - pos);
}

int main() {
    std::string api_key = get_api_key();
    if (api_key.empty()) {
        std::cerr << "No XAI_API_KEY in environment and no ~/.config/xai/api_key file found.\n"
                  << "Create the file with your key (chmod 600) or export XAI_API_KEY.\n";
        return 1;
    }

    std::vector<std::pair<std::string, std::string>> history;
    std::string line;

    std::cout << "\033[32m=== SECURE GROK CHAT (auto) ===\033[0m\nType 'exit' to quit.\n\n";

    while (true) {
        std::cout << "\033[36mYou:\033[0m " << std::flush;
        if (!std::getline(std::cin, line)) break;
        line = sanitize(line);
        if (line.empty()) continue;
        if (line == "exit" || line == "quit") break;

        history.emplace_back("user", line);

        std::string json = R"({"model":"grok-2-latest","messages":[)";
        for (size_t i = 0; i < history.size(); ++i) {
            if (i > 0) json += ",";
            json += R"({"role":")" + history[i].first + R"(","content":")" + history[i].second + R"("})";
        }
        json += "]}";

        CURL* curl = curl_easy_init();
        if (!curl) continue;

        std::string response;
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, "https://api.x.ai/v1/chat/completions");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 90L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Grok16-Secure-Client/1.0");

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "Request failed: " << curl_easy_strerror(res) << "\n";
            history.pop_back();
            continue;
        }

        std::string reply = extract_content(response);
        history.emplace_back("assistant", reply);
        if (history.size() > 16) history.erase(history.begin(), history.begin() + 2);

        std::cout << "\033[35mGrok:\033[0m " << reply << "\n\n";
    }

    std::cout << "Session closed.\n";
    return 0;
}
