#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
    struct stat flag{};
    if(stat("no-health.flag", &flag) == 0) return 4;
    for(int i = 1; i + 1 < argc; ++i) {
        if(std::string(argv[i]) == "--health-file") {
            std::ofstream health(argv[i + 1]);
            health << "{\"pid\":" << getpid() << "}";
        }
    }
    sleep(3);
    return 0;
}
