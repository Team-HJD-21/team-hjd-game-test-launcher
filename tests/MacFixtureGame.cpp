#include <fstream>
#include <string>
#include <sstream>
#include <regex>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
    struct stat flag{};
    if(stat("no-health.flag", &flag) == 0) return 4;
    for(int i = 1; i + 1 < argc; ++i) {
        if(std::string(argv[i]) == "--health-file") {
            std::ofstream health(argv[i + 1]);
            std::ifstream receipt("._teamhjd-receipt.json");std::stringstream bytes;bytes<<receipt.rdbuf();const auto raw=bytes.str();std::smatch match;std::string version;
            if(std::regex_search(raw,match,std::regex("\"version\"\\s*:\\s*\"([0-9A-Za-z._-]+)\"")))version=match[1];
            health << "{\"pid\":" << getpid() << ",\"version\":\"" << version << "\"}";
        }
    }
    sleep(3);
    return 0;
}
