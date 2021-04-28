#pragma once

#include "tools.hpp"

typedef enum {
    TT_NONE,
    TT_KEYWORD,
    TT_SYMBOL,
    TT_NUMBER,
    TT_OPEN_PAREN,
    TT_CLOSE_PAREN,
    TT_OPEN_CURLY,
    TT_CLOSE_CURLY,
    TT_SEMI_COLON,
    TT_COMMENT,
    TT_NEW_LINE,
    TT_WHITESPACE,
    TT_DOUBLE_QUOTE
} token_type_t;

struct token_t {
    char *at;
    uint32_t length;
    token_type_t type;
};

// Initialises string tree traversal machine
void tokeniser_init(
    uint32_t *keyword_ids,
    const char **keywords,
    uint32_t keyword_count);

token_t *tokenise(
    char *input,
    char comment_character,
    uint32_t *token_count);

void clear_tokeniser();
