#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// Constants
#define INPUT_FILE_BLOCK_SIZE       4096
#define LITERAL_STRING_MAX_SIZE     1024
#define GENERAL_NAME_MAX_SIZE       1024       

// Return codes codes
#define LEXER_OK                                0
#define LEXER_ERROR_INCORRECT_USAGE             1
#define LEXER_ERROR_FILE_IO                     2
#define LEXER_ERROR_IDENTIFIER_NAME_TOO_LONG    3
#define LEXER_ERROR_STRING_LITERAL_TOO_LONG     4
#define LEXER_ERROR_WRONG_INTEGER32_FORMAT      5
#define LEXER_ERROR_UPPERCASE_BOOLEAN_KEYWORD   6
#define LEXER_ERROR_INVALID_CHARACTER           7
#define LEXER_ERROR_INVALID_STRING_CHARACTER    8
#define LEXER_ERROR_NON_ESCAPED_NEWLINE         9

// Size of a static C-style array. Don't use on pointers!
#define ARRAYSIZE(_ARR)             ((int)(sizeof(_ARR) / sizeof(*(_ARR))))

/**
 * @brief ReadBuffer struct.
 * Automatically manages rebuffering, line counting and lookaheads.
 * 
 */
typedef struct ReadBuffer {
    FILE* fp;
    const char* filename;
    uint8_t content[INPUT_FILE_BLOCK_SIZE];

    size_t current_position;
    size_t total_size;
    size_t current_line;
} ReadBuffer;

/**
 * @brief Initializes the Read Buffer with the first 4096 bytes from in_file.
 * 
 * @param in_file File to read.
 * @return ReadBuffer with file contents.
 */
ReadBuffer init_buffer(FILE* in_file, const char* in_filename) {
    ReadBuffer buf = {.fp = in_file, .filename = in_filename, .current_position = 0, .current_line = 1};

    buf.total_size = fread(buf.content, 1, INPUT_FILE_BLOCK_SIZE, buf.fp);
    return buf;
}

/**
 * @brief Returns the next char in the buffer AND advances the internal position tracker.
 * If there aren't any more chars in the buffer, the file is automatically read again to refill the buffer.
 * 
 * @param buf Read buffer.
 * @return Next char in the buffer, if the end of file is reached, returns EOF.
 */
char next_char(ReadBuffer* buf) {
    if (buf->current_position == buf->total_size) {
        buf->total_size = fread(buf->content, 1, INPUT_FILE_BLOCK_SIZE, buf->fp);
        buf->current_position = 0;

        // End of file
        if (buf->total_size == 0)
            return EOF;
    }

    if (buf->content[buf->current_position] == '\n')
        buf->current_line++;

    return buf->content[buf->current_position++];
}

/**
 * @brief Returns the next character in the buffer. DOES NOT advance the internal position tracker.
 * Therefore, this function can be used to look ahead one character.
 * If there aren't any more chars in the buffer, the file is automatically read again to refill the buffer.
 * 
 * @param buf Read buffer.
 * @return Next char in the buffer, if the end of file is reached, returns EOF.
 */
char next_char_lookup(ReadBuffer* buf) {
    if (buf->current_position == buf->total_size) {
        fpos_t old_pos;
        fgetpos(buf->fp, &old_pos);

        char next_char = fgetc(buf->fp);

        if (next_char == EOF)
            return EOF;

        fsetpos(buf->fp, &old_pos);

        return next_char;
    }

    return buf->content[buf->current_position];
}

/**
 * @brief Returns the current character in the buffer. DOES NOT advance the internal position tracker.
 * Therefore, this function can be used to lookup the current character.
 * If there aren't any characters available, returns EOF.
 * 
 * @param buf Read buffer.
 * @return Current char in the buffer or EOF as stated above.
 */
char current_char_lookup(ReadBuffer* buf) {
    if (buf->current_position == 0)
        return EOF;

    return buf->content[buf->current_position - 1];
}

int iswhitespace(char c) {
    if (c == ' ' || c == '\n' || c == '\f' || c == '\r' || c == '\t' || c == '\v')
        return 1;

    return 0;
}

int isname(char c) {
    if (isalnum(c) || c == '_')
        return 1;

    return 0;
}

void extract_general_name(ReadBuffer* read_buffer, char* name_buffer) {
    // Get char that triggered this function call
    name_buffer[0] = current_char_lookup(read_buffer);

    size_t current_buffer_pos = 1;

    while (1) {
        if (!isname(next_char_lookup(read_buffer)))
            break;

        if (current_buffer_pos == GENERAL_NAME_MAX_SIZE) {
            printf("%s:%lld: \33[31mERROR:\33[0m identifier or keyword name too long (max %d chars allowed)\n",
                   read_buffer->filename,
                   read_buffer->current_line,
                   GENERAL_NAME_MAX_SIZE);

            exit(LEXER_ERROR_IDENTIFIER_NAME_TOO_LONG);
        }
        
        name_buffer[current_buffer_pos++] = next_char(read_buffer);
    }

    // Mark end
    name_buffer[current_buffer_pos] = '\0';
}

void extract_string(ReadBuffer* read_buffer, char* name_buffer) {
    size_t current_buffer_pos = 0;

    while (1) {
        if (current_buffer_pos == LITERAL_STRING_MAX_SIZE) {
            printf("%s:%lld: \33[31mERROR:\33[0m literal string too long (max %d chars allowed)\n",
                   read_buffer->filename,
                   read_buffer->current_line,
                   LITERAL_STRING_MAX_SIZE);

            exit(LEXER_ERROR_STRING_LITERAL_TOO_LONG);
        }

        char previous_char = current_char_lookup(read_buffer);
        char current_char = next_char(read_buffer);

        // Check end of string (also works for empty strings)
        // TODO: Fix bug when string is "anything\\"
        if (previous_char != '\\' && current_char == '\"')
            break;

        // Check invalid
        if (current_char == '\0' || current_char == EOF) {
            printf("%s:%lld: \33[31mERROR:\33[0m literal string may not contain null character or EOF\n",
                   read_buffer->filename,
                   read_buffer->current_line);

            exit(LEXER_ERROR_INVALID_STRING_CHARACTER);
        }

        // Multiline string
        if (next_char_lookup(read_buffer) == '\n') {
            if (current_char == '\\') {
                // Consume end of line
                next_char(read_buffer);
                continue;
            } else {
                // Incorrect multiline
                printf("%s:%lld: \33[31mERROR:\33[0m non-escaped newline character inside literal string.\n"
                       "\33[36mHINT:\33[0m add \\ before newline or close this string with \"\n",
                       read_buffer->filename,
                       read_buffer->current_line);

                exit(LEXER_ERROR_NON_ESCAPED_NEWLINE);
            }
        }

        // Valid char
        name_buffer[current_buffer_pos++] = current_char;
    }

    // Mark end
    name_buffer[current_buffer_pos] = '\0';
}

const char* extract_terminal(ReadBuffer* read_buffer) {
    // Get char that triggered this function call
    switch (current_char_lookup(read_buffer)) {
    case '(':
        return "lparen";
        break;

    case ')':
        return "rparen";
        break;

    case '*':
        return "times";
        break;

    case '+':
        return "plus";
        break;

    case ',':
        return "comma";
        break;

    case '-':
        return "minus";
        break;

    case '.':
        return "dot";
        break;

    case '/':
        return "divide";
        break;

    case ':':
        return "colon";
        break;

    case ';':
        return "semi";
        break;

    case '<':
        switch (next_char_lookup(read_buffer)) {
        case '-':
            next_char(read_buffer);
            return "larrow";
            break;
        
        case '=':
            next_char(read_buffer);
            return "le";
            break;

        default:
            return "lt";
            break;
        }
        break;

    case '=':
        switch (next_char_lookup(read_buffer)) {
        case '>':
            next_char(read_buffer);
            return "rarrow";
            break;

        default:
            return "equals";
            break;
        }
        break;

    case '@':
        return "at";
        break;

    case '{':
        return "lbrace";
        break;

    case '}':
        return "rbrace";
        break;

    case '~':
        return "tilde";
        break;
    }

    return NULL;
}

const char* check_keyword(char* text) {
    static const char* keywords[] = {"class", "else", "false", "fi", "if", 
                                     "in", "inherits", "isvoid", "let", "loop", 
                                     "pool", "then", "while", "case", "esac", 
                                     "new", "of", "not", "true"};

    // Convert to lowercase for case insensitive keywords 
    size_t text_size = strlen(text) + 1;
    char text_lower[text_size]; // This needs C99

    for (size_t i = 0; i < text_size; i++)
        text_lower[i] = tolower(text[i]);
    
    // Try to find keyword
    for (int i = 0; i < ARRAYSIZE(keywords); i++) {
        if (strcmp(text_lower, keywords[i]) == 0) {
            return keywords[i];
        }
    }

    return NULL;
}

int check_integer(char* text) {
    size_t text_size = strlen(text);

    // Check 1: no longer than 10 chars
    if (text_size > 10)
        return 0;

    // Check 2: only digits
    for (size_t i = 0; i < text_size; i++)
        if (!isdigit(text[i]))
            return 0;

    // Check 3: if text size is 10, first digit cannot be bigger than 2
    if (text_size == 10 && text[0] > '2')
        return 0;
        
    // Now we can safely convert the number
    // It can always fit under a UINT32 since its max value is 2999999999
    uint32_t num = 0;

    for (int32_t i = text_size - 1, power = 1; i >= 0; i--, power *= 10)
        num += (text[i] - 48) * power;

    // Check 4: no bigger than INT32_MAX
    return num <= INT32_MAX;
}

int remove_comments(ReadBuffer* read_buffer) {
    // Get current char again (we are not inside the main while loop)
    char current_char = current_char_lookup(read_buffer);

    // Handling one line comments
    if (current_char == '-' && next_char_lookup(read_buffer) == '-') {
        do {
            current_char = next_char(read_buffer);
        } while (current_char != '\n' && current_char != EOF);

        return 1;
    }

    // Handling multiple line comments
    if (current_char == '(' && next_char_lookup(read_buffer) == '*') {
        while (current_char != EOF) {
            if (current_char == '*' && next_char_lookup(read_buffer) == ')')
                break;
            
            current_char = next_char(read_buffer);
        }

        // Now, next_char is ')' or EOF, so it must be removed (for EOF nothing happens)
        next_char(read_buffer);

        return 1;
    }

    return 0;
}

void lexer(FILE* fp, const char* filename) {
    // Output file
    char out_filename[strlen(filename) + 5];

    strcpy(out_filename, filename);
    strcat(out_filename, "-lex");

    FILE* fp_lex = fopen(out_filename, "wb");

    if (fp_lex == NULL) {
        printf("\33[31mERROR:\33[0m could not open output file %s\n", out_filename);
        exit(LEXER_ERROR_FILE_IO);
    }

    // Buffer data
    ReadBuffer read_buffer = init_buffer(fp, filename);
    static char name_buffer[LITERAL_STRING_MAX_SIZE + 1];
    
    while (1) {
        char current_char = next_char(&read_buffer);

        if (current_char == EOF)
            break;

        // Ignore whitespace and comments
        if (iswhitespace(current_char))
            continue;

        if (remove_comments(&read_buffer))
            continue;

        // Now we can find something
        fprintf(fp_lex, "%lld\n", read_buffer.current_line);

        // Strings
        if (current_char == '\"') {
            extract_string(&read_buffer, name_buffer);

            fprintf(fp_lex, "string\n%s\n", name_buffer);
            continue;
        }

        // Not a string and not a name, might be a terminal
        if (!isname(current_char)) {
            const char* terminal_name = extract_terminal(&read_buffer);

            if (terminal_name == NULL) {
                printf("%s:%lld: \33[31mERROR:\33[0m invalid character %c\n",
                    read_buffer.filename,
                    read_buffer.current_line,
                    current_char);

                exit(LEXER_ERROR_INVALID_CHARACTER);
            }

            fprintf(fp_lex, "%s\n", terminal_name);
            continue;
        }

        // None of the above, handle everything else (keywords, identifiers, type identifiers, integers)
        extract_general_name(&read_buffer, name_buffer);

        // Integers
        if (isdigit(name_buffer[0])) {
            if (check_integer(name_buffer) == 1) {
                fprintf(fp_lex, "integer\n%s\n", name_buffer);
                continue;
            }

            printf("%s:%lld: \33[31mERROR:\33[0m %s is not a positive 32-bit signed integer (max value allowed %d)\n",
                    read_buffer.filename,
                    read_buffer.current_line,
                    name_buffer,
                    INT32_MAX);

            exit(LEXER_ERROR_WRONG_INTEGER32_FORMAT);
        }

        // Check keywords
        const char* keyword = check_keyword(name_buffer);

        if (keyword != NULL) {
            // Test for true and false special case
            if (strcmp(keyword, "true") || strcmp(keyword, "false")) {
                if (name_buffer[0] >= 'A' && name_buffer[0] <= 'Z') {
                    printf("%s:%lld: \33[31mERROR:\33[0m keyword %s may not start with a capital letter\n",
                        read_buffer.filename,
                        read_buffer.current_line,
                        keyword);

                    exit(LEXER_ERROR_UPPERCASE_BOOLEAN_KEYWORD);
                }
            }

            fprintf(fp_lex, "%s\n", keyword);
            continue;
        }   
        
        // Check Type
        if (name_buffer[0] >= 'A' && name_buffer[0] <= 'Z') {
            fprintf(fp_lex, "type\n%s\n", name_buffer);
            continue;
        }

        // Last case: identifier
        fprintf(fp_lex, "identifier\n%s\n", name_buffer);
    }

    fclose(fp_lex);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("\33[31mERROR:\33[0m expected usage: %s [file]\n", argv[0]);
        return LEXER_ERROR_INCORRECT_USAGE;
    }

    FILE* fp = fopen(argv[1], "rb");

    if (fp == NULL) {
        printf("\33[31mERROR:\33[0m could not open file %s\n", argv[1]);
        return LEXER_ERROR_FILE_IO;
    }

    // Start lexical analysis
    lexer(fp, argv[1]);

    fclose(fp);

    return LEXER_OK;
}