// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#include<functional>
#include<string>
#include <vector>
#include <filesystem>
#include <cmath>
#include <map>
#include <sys/wait.h>
#include <cstring>
#include <fcntl.h>
#include <cassert>

/*
 * CONFIG SECTION
 * - ADJUST CODE BELOW
 */

#define MAX_NUMBER_OF_PROCESSES 16

std::filesystem::path pwd;
std::filesystem::path srcdir;
std::filesystem::path builddir;
std::filesystem::path bas55;

pid_t exec(std::vector<std::string> comp);

/*
 * First thing called in the program
 */
void init() {
    pwd = getenv("PWD");
    if (pwd.empty()) {
        throw std::runtime_error("cannot locate test directory");
    }
    srcdir = pwd / "tests";
    builddir = pwd / "cmake-build-debug" / "tests";
    bas55 = pwd / "run.sh";
    setenv("srcdir", srcdir.c_str(), 1);
    setenv("builddir", builddir.c_str(), 1);
    setenv("bas55", bas55.c_str(), 1);
}

using RETVAL = std::vector<std::pair<std::function<pid_t(void)>, std::function<std::string()>>>;

RETVAL enumerateTestCases() {
    RETVAL ret{};
    for (auto &f: std::filesystem::directory_iterator(srcdir)) {
        auto &p = f.path();
        if (p.has_extension()) {
            if (p.extension() == ".test") {
                ret.emplace_back([p]() { return exec({"timeout", "10", p}); }, [p]() { return (--p.end())->string(); });
            }
        }
    }
    return ret;
}

/*
 * END OF CONFIG SECTION
 */

int child_processes_active = 0;

pid_t exec(std::vector<std::string> comp) {
    pid_t f = fork();
    switch(f) {
        case 0: {
            int null = open("/dev/null", O_RDWR);
            if(errno) {
                perror("open");
            }
            dup2(null, 0);
            if(errno) {
                perror("dup2");
            }
            dup2(null, 1);
            if(errno) {
                perror("dup2");
            }
            // dup2(null, 2);
            std::vector<const char *> d;
            d.reserve(comp.size() + 1);
            for (auto &c: comp) {
                d.push_back(c.c_str());
            }
            d.push_back(nullptr);
            execvp(d[0], const_cast<char *const *>(d.data()));
            perror("execvp");
            exit(EXIT_FAILURE);
        }
        case -1:
            throw std::runtime_error(strerror(errno));
        default:
            return f;
    }
}


#define REDBG "\x1b[41m"
#define REDFG "\x1b[31m"
#define GREEN "\x1b[32m"
#define RST "\x1b[0m"

void fail(const char *n, const char *msg) {
    printf(REDBG
           "%4s"
           RST
           " %s\n", n, msg);
}

void good(const char *msg) {
    printf(GREEN
           "GOOD"
           RST
           " %s\n", msg);
}

void screen_list(RETVAL &cases) {
    std::map<pid_t, std::function<std::string()>> processes{};
    std::map<std::string, std::string> statuses {};
    auto case_iter = cases.begin();
    auto case_end = cases.end();
    while (!processes.empty() || case_iter != case_end) {
        while(case_iter != case_end) {
            if (child_processes_active >= MAX_NUMBER_OF_PROCESSES) break;
            auto [ex, txt] = *case_iter;
            try {
                auto e = ex();
                assert(!processes.contains(e));
                processes[e] = txt;
            } catch (std::runtime_error &err) {
                fail("FAIL", err.what());
            }
            ++child_processes_active;
            ++case_iter;
        }
        int wstatus;
        auto w = waitpid(-1, &wstatus, 0);
        if (w == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        auto s = processes.at(w)();

        if (WIFEXITED(wstatus)) {
            if (WEXITSTATUS(wstatus)) {
                std::string sn = std::to_string(WEXITSTATUS(wstatus));
                statuses[s] = sn;
                fail(sn.c_str(), s.c_str());
            } else {
                statuses[s] = "GOOD";
                good(s.c_str());
            }
        } else if (WIFSIGNALED(wstatus)) {
            auto sig = WTERMSIG(wstatus);
            const char *n = "FAIL";
            switch (sig) {
                case SIGHUP:
                    n = "HUP";
                    break;
                case SIGINT:
                    n = "INT";
                    break;
                case SIGQUIT:
                    n = "QUIT";
                    break;
                case SIGILL:
                    n = "ILL";
                    break;
                case SIGTRAP:
                    n = "TRAP";
                    break;
                case SIGABRT:
                    n = "ABRT";
                    break;
                case SIGBUS:
                    n = "BUS";
                    break;
                case SIGFPE:
                    n = "FPE";
                    break;
                case SIGKILL:
                    n = "KILL";
                    break;
                case SIGUSR1:
                    n = "USR1";
                    break;
                case SIGSEGV:
                    n = "SEGV";
                    break;
                case SIGUSR2:
                    n = "USR2";
                    break;
                case SIGPIPE:
                    n = "PIPE";
                    break;
                case SIGALRM:
                    n = "ALRM";
                    break;
                case SIGTERM:
                    n = "TERM";
                    break;
                case SIGSTKFLT:
                    n = "STKFLT";
                    break;
                case SIGCHLD:
                    n = "CHLD";
                    break;
                case SIGCONT:
                    n = "CONT";
                    break;
                case SIGSTOP:
                    n = "STOP";
                    break;
                case SIGTSTP:
                    n = "TSTP";
                    break;
                case SIGTTIN:
                    n = "TTIN";
                    break;
                case SIGTTOU:
                    n = "TTOU";
                    break;
                case SIGURG:
                    n = "URG";
                    break;
                case SIGXCPU:
                    n = "XCPU";
                    break;
                case SIGXFSZ:
                    n = "XFSZ";
                    break;
                case SIGVTALRM:
                    n = "VTALRM";
                    break;
                case SIGPROF:
                    n = "PROF";
                    break;
                case SIGWINCH:
                    n = "WINCH";
                    break;
                case SIGIO:
                    n = "IO";
                    break;
                case SIGPWR:
                    n = "PWR";
                    break;
                case SIGSYS:
                    n = "SYS";
                    break;
                default:
                    break;
            }
            statuses[s] = n;
            fail(n, s.c_str());
        } else if (WIFSTOPPED(wstatus) || WIFCONTINUED(wstatus)) {
            continue;
        }
        child_processes_active = std::max(child_processes_active - 1, 0);
        processes.erase(w);
    }

    if(std::all_of(statuses.cbegin(), statuses.cend(), [](auto p) {
        return strcmp(p.second.c_str(), "GOOD") == 0;
    })) {
        printf("\n");
        printf(GREEN "ALL TESTS PASSED" RST "\n");
        printf("\n");
    } else {
        printf("\n");
        printf(REDFG "SOME TESTS FAILED" RST "\n");
        long e_count = 0;
        for(auto &[txt, st] : statuses) {
            if(strcmp(st.c_str(), "GOOD") != 0) {
                printf(REDBG "%4s" RST " %s" RST "\n", st.c_str(), txt.c_str());
                ++e_count;
            }
        }
        printf("\n");
        printf(REDFG "%li/%li TESTS FAILED" RST "\n", e_count, statuses.size());
    }
}

void screen_grid(RETVAL &cases, long h, long w) {
    screen_list(cases);
}

int
main() {
    init();

    auto cases = enumerateTestCases();

    if (cases.empty()) {
        fprintf(stderr, "no test cases found\n");
        exit(EXIT_FAILURE);
    }

    screen_list(cases);
}