#include "shell.h"
#include <iostream>
#include <string>
#include <cstring>

static void print_usage() {
    std::cout << "Usage: minibash [OPTIONS] [script]\n"
              << "  -c 'cmd'   Execute command string and exit\n"
              << "  -i         Force interactive mode\n"
              << "  --help     Show this help\n"
              << "  script     Run script file and exit\n\n"
              << "Without arguments, runs interactively.\n";
}

int main(int argc, char* argv[]) {
    Shell shell;

    if (argc == 1) {
        shell.run_interactive();
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];

        if (a == "--help" || a == "-h") {
            print_usage(); return 0;
        }
        if (a == "-c" && i + 1 < argc) {
            int ret = shell.run_line(argv[++i]);
            return ret;
        }
        if (a == "-i") {
            shell.run_interactive();
            return 0;
        }
        if (a[0] != '-') {
            // Script mode
            return shell.run_script(a);
        }
    }

    return 0;
}
