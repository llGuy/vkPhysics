#include "tokeniser.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "allocators.hpp"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

struct token_t *tokens_buffer;

struct string_tree_node_data_t {
    struct {
        uint64_t data : 16;
        uint64_t string_length : 16;
        uint64_t initialised : 1;
        uint64_t hash : 32;
    };
    const char *string;

    struct string_tree_node_t *next;
};

struct string_tree_node_t {
    string_tree_node_data_t nodes[127];
};

static uint32_t s_hash_impl(
    char *buffer,
    size_t buflength) {
    uint32_t s1 = 1;
    uint32_t s2 = 0;

    for (size_t n = 0; n < buflength; n++) {
        s1 = (s1 + buffer[n]) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    return (s2 << 16) | s1;
}

static uint32_t s_hash(
    char *string,
    uint32_t length) {
    return s_hash_impl(string, length - 1);
}

static string_tree_node_t *s_tree_node_create() {
    string_tree_node_t *new_node = (string_tree_node_t *)malloc(sizeof(string_tree_node_t));
    memset((void *)new_node, 0, sizeof(string_tree_node_t));
    return new_node;
}

static void s_string_data_init(
    uint32_t data, 
    const char *string,
    uint32_t string_length, 
    uint32_t hash, 
    string_tree_node_data_t *node_data) {
    node_data->initialised = 1;
    node_data->data = data;
    node_data->hash = hash;
    node_data->string = string;
    node_data->string_length = string_length;
}

static void s_string_data_deinit(
    string_tree_node_data_t *node_data) {
    node_data->initialised = 0;
    node_data->data = 0;
    node_data->hash = 0;
    node_data->string = NULL;
    node_data->string_length = 0;
}

static void s_handle_conflict(
    uint32_t current_char,
    const char *string, 
    uint32_t length,
    uint32_t hash,
    uint32_t data,
    string_tree_node_t *current,
    string_tree_node_data_t *string_data) {
    string_tree_node_data_t smaller, bigger;

    if (string_data->string_length < length) {
        s_string_data_init(string_data->data, string_data->string, string_data->string_length, string_data->hash, &smaller);
        s_string_data_init(data, (char *)string, length, hash, &bigger);
    }
    else {
        s_string_data_init(data, (char *)string, length, hash, &smaller);
        s_string_data_init(string_data->data, string_data->string, string_data->string_length, string_data->hash, &bigger);
    }

    s_string_data_deinit(string_data);
    
    string_tree_node_t *current_node = current;
    for (uint32_t i = current_char; i < smaller.string_length; i++) {
        if (smaller.string[i] == bigger.string[i]) {
            // Need to do process again
            if (i == smaller.string_length - 1) {
                // End
                // This is the smaller string's final node
                s_string_data_init(smaller.data, smaller.string, smaller.string_length, smaller.hash, &current_node->nodes[smaller.string[i]]);

                current_node->nodes[bigger.string[i]].next = s_tree_node_create();
                current_node = current_node->nodes[bigger.string[i]].next;
                s_string_data_init(bigger.data, bigger.string, bigger.string_length, bigger.hash, &current_node->nodes[bigger.string[i + 1]]);
                
                break;
            }
            
            current_node->nodes[bigger.string[i]].next = s_tree_node_create();
            current_node = current_node->nodes[bigger.string[i]].next;
        }
        else {
            // No more conflicts : can dump string in their respective character slots
            s_string_data_init(smaller.data, smaller.string, smaller.string_length, smaller.hash, &current_node->nodes[smaller.string[i]]);
            s_string_data_init(bigger.data, bigger.string, bigger.string_length, bigger.hash, &current_node->nodes[bigger.string[i]]);

            break;
        }
    }
}

static void s_register_string(
    string_tree_node_t *root, 
    const char *string, 
    uint32_t length,
    uint32_t data) {
    string_tree_node_t *current = root;
    uint32_t hash = s_hash((char *)string, length);
    string_tree_node_data_t *string_data = &current->nodes[string[0]];

    uint32_t i = 0;
    for (; i < length && current->nodes[string[i]].next; ++i) {
        string_data = &current->nodes[string[i]];
        current = string_data->next;
    }

    // The symbol already exists
    if (string_data->hash == hash) {
        exit(1);
    }

    // If string has already been initialised
    if (string_data->initialised) {
        // Problem: need to move the current string to another node
        s_handle_conflict(
            i,
            string,
            length,
            hash,
            data,
            current,
            string_data);

        return;
    }

    s_string_data_init(data, string, length, hash, string_data);
}

static string_tree_node_data_t *s_traverse_tree(
    string_tree_node_t *root,
    const char *string,
    uint32_t length) {
    string_tree_node_t *current = root;
    uint32_t hash = s_hash((char *)string, length);

    for (uint32_t i = 0; i < length && current; ++i) {
        if (current->nodes[string[i]].hash == hash) {
            return &current->nodes[string[i]];
        }
        
        current = current->nodes[string[i]].next;
    }

    return NULL;
}

static token_type_t s_determine_character_token_type(
    char c) {
    switch(c) {
        
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': return TT_NUMBER;
    case '(': return TT_OPEN_PAREN;
    case ')': return TT_CLOSE_PAREN;
    case '{': return TT_OPEN_CURLY;
    case '}': return TT_CLOSE_CURLY;
    case ';': return TT_SEMI_COLON;
    case '\"': return TT_DOUBLE_QUOTE;
    case ' ': case '\t': return TT_WHITESPACE;
    case '\n': return TT_NEW_LINE;
    default: return TT_NONE;

    }
}

static string_tree_node_t *root;

void tokeniser_init(
    uint32_t *keyword_ids,
    const char **keywords,
    uint32_t keyword_count) {
    tokens_buffer = LN_MALLOC(token_t, 1000);
    root = s_tree_node_create();

    for (uint32_t i = 0; i < keyword_count; ++i) {
        s_register_string(root, keywords[i], strlen(keywords[i]), keyword_ids[i]);
    }
}

token_t *tokenise(
    char *input,
    char comment_character,
    uint32_t *token_count) {
    // Tokenise string for easy 
    token_t *tokens = tokens_buffer;
    *token_count = 0;

    char *current_whitespace_start = NULL;
    
    char *word_start = NULL;
    char *number_start = NULL;

    for (char *c = input; *c != 0; ++c) {
        token_type_t type = s_determine_character_token_type(*c);
        if (*c == comment_character) {
            type = TT_COMMENT;
        }

        if (word_start && (type != TT_NONE && type != TT_NUMBER)) {
            // Add a new token
            token_t token = {};
            token.at = word_start;
            token.length = c - word_start;

            string_tree_node_data_t *keyword = s_traverse_tree(root, token.at, token.length);
            if (keyword) {
                token.type = (token_type_t)keyword->data;
            }
            else {
                token.type = TT_NONE;
            }

            tokens[*token_count] = token;
            ++(*token_count);

            word_start = NULL;
        }

        if (number_start && type != TT_NUMBER) {
            token_t token = {};
            token.at = number_start;
            token.length = c - number_start;
            token.type = TT_NUMBER;

            tokens[*token_count] = token;
            ++(*token_count);

            number_start = NULL;
        }

        switch(type) {
        case TT_NEW_LINE: {
            token_t token = {};
            token.at = c;
            token.length = 1;
            token.type = TT_NEW_LINE;
            tokens[*token_count] = token;
            ++(*token_count);
        } break;

        case TT_COMMENT: {
            token_t token = {};
            token.at = c;
            token.length = 1;
            token.type = TT_COMMENT;
            tokens[*token_count] = token;
            ++(*token_count);
        } break;

        case TT_WHITESPACE: {
            token_t token = {};
            token.at = c;
            token.length = 1;
            token.type = TT_WHITESPACE;
            tokens[*token_count] = token;
            ++(*token_count);
        } break;

        case TT_NONE: {
            if (!word_start) {
                word_start = c;
            }
        } break;

        case TT_NUMBER: {
            if (!word_start) {
                if (!number_start) {
                    number_start = c;
                }
            }
        } break;

        case TT_DOUBLE_QUOTE: {
            token_t token = {};
            token.at = c;
            token.length = 1;
            token.type = TT_DOUBLE_QUOTE;
            tokens[*token_count] = token;
            ++(*token_count);
        } break;

        default: {

        } break;
        }
    }

    return tokens_buffer;
}
