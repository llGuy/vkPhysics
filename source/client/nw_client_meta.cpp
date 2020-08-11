#include <curl/curl.h>
#include <common/files.hpp>

void nw_check_registration() {
    file_handle_t file_handle = create_file("assets/.user_meta", FLF_TEXT);
    file_contents_t contents = read_file(file_handle);

    // .user_meta should contain user id, user tag and user name
    // the user id can not be 0 - if it is, we haven't registered yet

    if (contents.data[0] == '\0') {
        
    }
}
