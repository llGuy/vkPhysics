#include "log.hpp"
#include <stdio.h>
#include "files.hpp"

FILE *g_logfile;

void init_log_file() {
    g_logfile = fopen(create_real_path("tools/log.txt"), "w+");
}
