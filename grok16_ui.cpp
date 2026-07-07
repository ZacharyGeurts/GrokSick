#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <algorithm>
#include <cctype>

std::string sanitize(const std::string& in) {
    std::string s = in.substr(0, 1024);
    std::string out;
    for (unsigned char c : s) {
        if (std::iscntrl(c) && c != '\t') continue;
        out += c;
    }
    const std::vector<std::string> bad = {";","|","&","`","$","../","rm ","eval("};
    for (auto& p : bad) if (out.find(p) != std::string::npos) return "[BLOCKED]";
    return out;
}

std::vector<std::string> PROFILES = {"ai", "field_opt", "vulkan_rtx", "belt_2_0", "field_physics", "queen-rtx"};

bool run_cmd(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv.data());
        perror("exec failed");
        _exit(127);
    }
    int st; waitpid(pid, &st, 0);
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

int main() {
    std::string grok_root = std::getenv("HOME") ? std::string(std::getenv("HOME")) + "/Grok16" : ".";
    std::cout << "\n=== SECURE GROK16 BUILD UI ===\n"
              << "Grok16 root: " << grok_root << "\n"
              << "Profiles: ai | field_opt | vulkan_rtx | belt_2_0 | field_physics | queen-rtx\n"
              << "Type number or 'exit'\n\n";

    while (true) {
        std::cout << "[1] Build with profile   [2] Clean   [3] Exit\n> " << std::flush;
        std::string choice;
        if (!std::getline(std::cin, choice)) break;
        choice = sanitize(choice);
        if (choice == "3" || choice == "exit") break;

        if (choice == "2") {
            std::string b; std::cout << "Build dir to remove: "; std::getline(std::cin, b);
            b = sanitize(b);
            if (!b.empty()) run_cmd({"rm","-rf",b});
            continue;
        }

        if (choice == "1") {
            std::cout << "Profile: "; std::string prof; std::getline(std::cin, prof); prof = sanitize(prof);
            bool ok = false; for (auto& p : PROFILES) if (p == prof) ok = true;
            if (!ok) { std::cout << "Unknown profile\n"; continue; }

            std::string src, bld = "build", btype = "Release";
            std::cout << "Source dir: "; std::getline(std::cin, src); src = sanitize(src);
            if (src.empty()) continue;
            std::cout << "Build dir [" << bld << "]: "; std::string tmp; std::getline(std::cin, tmp);
            if (!tmp.empty()) bld = sanitize(tmp);
            std::cout << "Type [Release]: "; std::getline(std::cin, tmp);
            if (!tmp.empty()) btype = sanitize(tmp);

            std::vector<std::string> cargs = {
                "cmake", "-S", src, "-B", bld,
                "-DCMAKE_TOOLCHAIN_FILE=" + grok_root + "/cmake/grok16-toolchain.cmake",
                "-DCMAKE_PROJECT_INCLUDE=" + grok_root + "/cmake/grok16-profile-" + prof + ".cmake",
                "-DCMAKE_BUILD_TYPE=" + btype
            };
            std::cout << "Running cmake with Grok16 (" << prof << ")...\n";
            if (run_cmd(cargs)) {
                run_cmd({"make", "-C", bld, "-j" + std::to_string(8)});
                std::cout << "Build finished.\n";
            } else {
                std::cout << "CMake step failed.\n";
            }
        }
    }
    std::cout << "Session closed.\n";
    return 0;
}
