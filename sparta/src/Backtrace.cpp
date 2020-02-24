


// Code borrowed from:
// http://stackoverflow.com/questions/77005/how-to-generate-a-stacktrace-when-my-gcc-c-app-crashes
//
// See also:
// http://www.gnu.org/software/libc/manual/html_node/Backtraces.html
// http://www.linuxjournal.com/article/6391

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif

#define _XOPEN_SOURCE

#include <execinfo.h>
#include <signal.h>
#include <cstring>
#include <cxxabi.h>
#include <cassert>
#ifndef __APPLE__
#include <features.h>
#if __GLIBC_PREREQ(2,26)
// siginfo_t.h refactors some macros out of signal.h since glibc v2.26
#include <bits/types/siginfo_t.h>
#endif
#endif
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <exception>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "sparta/utils/Colors.hpp"
#include "sparta/app/Backtrace.hpp"
#include "sparta/app/SimulationInfo.hpp"
#include "sparta/utils/SpartaException.hpp"

namespace sparta {
    namespace app {

/* This structure mirrors the one found in /usr/include/asm/ucontext.h */
// typedef struct _sig_ucontext {
//     unsigned long     uc_flags;
//     struct ucontext   *uc_link;
//     stack_t           uc_stack;
//     struct sigcontext uc_mcontext;
//     sigset_t          uc_sigmask;
// } sig_ucontext_t;


const char* backtrace_demangle(const char* msg, char* buf, size_t buf_size)
{

    const uint32_t SYMBOL_BUF_SIZE = 4096;
    char SYMBOL_BUF[SYMBOL_BUF_SIZE];
    uint32_t symbol_pos = 0;
    uint32_t state = 0;
    char* _buf = buf;

    if(buf_size == 0){
        return msg;
    }

    while(1){
        if((size_t)(_buf - buf) >= buf_size - 1){
            buf[buf_size-1] = '\0';
            return buf;
        }

        char in = *(msg++);
        if(in == '\0'){
            assert((size_t)(_buf - buf) < buf_size);
            *(_buf++) = '\0';
            return buf;
        }

        switch(state){
        case 0:
            if(in == '('){
                state++;
            }
            *(_buf++) = in;
            assert((size_t)(_buf - buf) < buf_size);
            break;
        case 1:
            if(in == '+'){
                SYMBOL_BUF[symbol_pos] = '\0';
                int status;
                //std::cerr << "demangling \"" << SYMBOL_BUF << "\"" << std::endl;
                size_t local_buf_size = buf_size - (_buf - buf);
                size_t demangled_size = 0;
                char* out = __cxxabiv1::__cxa_demangle(SYMBOL_BUF,
                                                       nullptr,
                                                       &demangled_size,
                                                       &status);
                //std::cerr << "   ==> \"" << out << "\"" << std::endl;
                (void) status;
                if(nullptr == out){
                    // demangle failed
                    if(symbol_pos >= local_buf_size){
                        // Truncate symbol string copied to buffer because buffer will be full
                        strncpy(_buf, SYMBOL_BUF, local_buf_size);
                        _buf += local_buf_size;
                         _buf[local_buf_size-1] = '\0';
                        return buf;
                    }else{
                        strcpy(_buf, SYMBOL_BUF); // demangle failed
                        _buf += symbol_pos;
                    }
                    assert((size_t)(_buf - buf) < buf_size);
                }else{
                    strncpy(_buf, out, local_buf_size); // Copy the output limited to the remainder of this buffer
                    free(out);
                    if(local_buf_size > demangled_size){
                        _buf[demangled_size] = '\0';
                    }else{
                        // Reached end of buffer with this demangling
                        _buf[local_buf_size-1] = '\0';
                        assert(_buf + local_buf_size == buf + buf_size);
                        //fprintf(stderr, "DONE Handling case 1 - early return\n");
                        //fprintf(stderr, "Returned buffer is (%iB): \"%s\"\n", (uint32_t)strlen(buf), buf);
                        return buf;
                    }
                    while(*(++_buf) != '\0'){}
                    assert((size_t)(_buf - buf) < buf_size);
                }
                symbol_pos = 0;
                *(_buf++) = in; // Write the char
                assert((size_t)(_buf - buf) < buf_size);
                state++;
            }else{
                // Stop capturing the symbol if it exceeds size
                if(symbol_pos < SYMBOL_BUF_SIZE){
                    SYMBOL_BUF[symbol_pos++] = in;
                }
            }
            break;
        case 2:
            *(_buf++) = in;
            assert((size_t)(_buf - buf) < buf_size);
        };
    }

    fprintf(stderr, "Returned buffer is: \"%s\"\n", buf);

    return buf;
}

// Note: leave this is a C function to avoid potential C++ calls during signal handling
void dump_backtrace(FILE* f, void* caller_address = nullptr)
{
    char**          messages;
    int             size, i;

    void* array[50];
    size = backtrace(array, 50);
    (void)caller_address;
    // Overwrite sigaction with caller's address
//     if(caller_address == nullptr){
//         // Grab the current stack address from this ucontext. This is different than the
//         // sig_ucontext_t. Read from the EIP (x86) or RIP (x86_64)
//         ucontext_t uc;
//         if(0 == getcontext(&uc)){
// #if defined __x86_64__
//             caller_address = (void *) uc.uc_mcontext.gregs[REG_RIP];
// #else
//             caller_address = (void *) uc.uc_mcontext.gregs[REG_EIP];
// #endif
//             array[1] = caller_address;
//         }
//     }else{
//         array[1] = caller_address;
//     }

    messages = backtrace_symbols(array, size);

    // Skip first stack frame (points here)
    char demangle_buf[4096];
    for (i = 1; i < size && messages != NULL; ++i){
        const char* bt_line = backtrace_demangle(messages[i], demangle_buf, sizeof(demangle_buf));
        fprintf(f, "(%2d) %s\n", i, bt_line);

        std::string prog = sparta::SimulationInfo::getInstance().executable;
        if(prog != ""){
            // Extract line information from addr2line. Fails silently if not present
            char syscom[256];
            sprintf(syscom, "addr2line %p -e %s", array[i], prog.c_str()); //last parameter is the name of this app
            FILE* fp = popen(syscom, "r");
            if(fp){
                fprintf(f, "     ");
                char copy_buf[512];
                size_t bread;
                while(bread = fread(copy_buf, 1, 512, fp), bread > 0){
                    fwrite(copy_buf, 1, bread, f);
                }
                pclose(fp);
            }
        }
    }

    free(messages);
}

BacktraceData get_backtrace(void* caller_address = nullptr)
{
    BacktraceData raw;

    char**          messages;
    volatile int    bt_size; // volatile fixes compiler error: 'bt_size’ might be clobbered by ‘longjmp’ or ‘vfork

    void* array[50];
    bt_size = backtrace(array, 50);

    (void) caller_address;
    // Overwrite sigaction with caller's address
//     if(caller_address == nullptr){
//         // Grab the current stack address from this ucontext. This is different than the
//         // sig_ucontext_t. Read from the EIP (x86) or RIP (x86_64)
//         ucontext_t uc;
//         if(0 == getcontext(&uc)){
// #if defined __x86_64__
//             caller_address = (void *) uc.uc_mcontext.gregs[REG_RIP];
// #else
//             caller_address = (void *) uc.uc_mcontext.gregs[REG_EIP];
// #endif
//             array[1] = caller_address;
//         }
//     }else{
//         array[1] = caller_address;
//     }

    messages = backtrace_symbols(array, bt_size);

    // Skip first stack frame (points here)
    if(messages){
        for (int i = 1; i < bt_size; ++i){
            raw.addFrame(array[i], messages[i]);
        }

        free(messages);
    }

    return raw;
}

void BacktraceData::render(std::ostream& o, bool line_info) const
{
    char demangle_buf[4096];

    uint32_t i = 1; // Begin with frame 1.
    for(auto& frame : frames_){
        void* addr = frame.first;
        const std::string& msg = frame.second;

        const char* bt_line = backtrace_demangle(msg.c_str(), demangle_buf, sizeof(demangle_buf));
        std::stringstream ss;
        ss << "(" << i << ")";
        o << std::setw(5) << ss.str() << " " << bt_line << "\n";

        if(line_info){
            std::string prog = sparta::SimulationInfo::getInstance().executable;
            if(prog != ""){
                // Extract line information from addr2line. Fails silently if not present
                char syscom[256];
                sprintf(syscom, "addr2line %p -e %s", addr, prog.c_str()); //last parameter is the name of this app
                FILE* fp = popen(syscom, "r");
                if(fp){
                    o << "      ";
                    char copy_buf[512];
                    size_t bread;
                    while(bread = fread(copy_buf, 1, 512, fp), bread > 0){
                        for(size_t j = 0; j < bread; ++j){
                            o << copy_buf[j];
                        }
                    }
                    pclose(fp);
                }
            }
        }
        i++;
    }
}

void BacktraceData::addFrame(void* addr, const char* message)
{
    frames_.emplace_back(addr, message);
}

void crit_err_hdlr(int sig_num, siginfo_t * info, void * ucontext)
{
    void*           caller_address = nullptr;
    (void) ucontext;
//     sig_ucontext_t* uc;

//     uc = (sig_ucontext_t *)ucontext;

//     // Get the address at the time the signal was raised from the EIP (x86) or
//     // RIP (x86_64)
// #if defined __x86_64__
//     caller_address = (void *) uc->uc_mcontext.rip;
// #else
//     caller_address = (void *) uc->uc_mcontext.eip;
// #endif

    fprintf(stderr,
            std::string(std::string(SPARTA_CURRENT_COLOR_BRIGHT_RED) + "Received Signal %d (%s), address is %p from %p\n" + std::string(SPARTA_CURRENT_COLOR_NORMAL)).c_str(),
            sig_num, strsignal(sig_num), info->si_addr, caller_address);

    dump_backtrace(stderr, caller_address);

    fprintf(stderr, "Exiting\n");
    exit(EXIT_FAILURE);
}

Backtrace::Backtrace() :
    sigact_()
{
    sigact_.sa_handler = nullptr;
    sigact_.sa_sigaction = crit_err_hdlr;
    sigemptyset(&sigact_.sa_mask);
    sigact_.sa_flags = SA_RESTART | SA_SIGINFO;
    //sigact_.sa_restorer = nullptr;
}

Backtrace::~Backtrace()
{
    for(auto& p : handled_){
        struct sigaction curact;
        int sig = p.first;
        bool restore = false;
        if(sigaction(sig, (struct sigaction*)nullptr, &curact) != 0){
            std::cerr << "Warning: Failed to check current sigaction for " << strsignal(sig)
                      << ". Restoring old action anyway" << std::endl;
            restore = true;
        }else{
            if(curact.sa_sigaction == sigact_.sa_sigaction){
                restore = true;
            }else{
                std::cerr << "Warning: When restoring signal handler for " << strsignal(sig)
                          << " the current handler differed from the handler that "
                          "sparta::app::Backtrace registered" << std::endl;
                restore = false;
            }
        }
        if(restore){
            if(sigaction(p.first, &p.second, (struct sigaction*)nullptr) != 0) {
                std::cerr << "Warning: Failed to restore current sigaction for " << strsignal(sig)
                          << ". Restoring old action anyway" << std::endl;
            }
        }
    }

    //std::cout << "Restored " << handled_.size() << " signal handlers" << std::endl;
}

void Backtrace::setAsHandler(int sig)
{
    struct sigaction oldact;
    if(sigaction(sig, &sigact_, &oldact) != 0){
        throw SpartaException("error setting signal handler for") << sig << strsignal(sig);
    }

    handled_[sig] = oldact;
}

void Backtrace::dumpBacktrace(std::ostream& o)
{
    char* buffer = nullptr;
    size_t size;
    FILE* f = open_memstream(&buffer, &size);
    if(!f){
        o << "<Unable to open memory stream to write backtrace>";
        return;
    }

    dump_backtrace(f, nullptr);

    fclose(f);

    // buffer is set only after the file is closed
    if(!buffer){
        std::cerr << "open_memstream did not yield a buffer when closed" << std::endl;
        std::terminate();
    }

    const char* pc = buffer;
    while(pc < buffer + size){
        if(*pc == '\0'){
            break;
        }
        o << *pc;
        pc++;
    }

    free(buffer);
}

BacktraceData Backtrace::getBacktrace()
{
    return get_backtrace(nullptr);
}

    } // namespace app
} // namespace sparta
