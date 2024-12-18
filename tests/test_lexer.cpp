#include "lexer.h"
#include <gtest/gtest.h>

// A helper function to initialize a lexer and retrieve a sequence of tokens.
// Returns all tokens until EOF is reached.
static std::vector<Token> tokenizeSource(const char* source) {
    Lexer lexer;
    lexer_init(&lexer, source);
    std::vector<Token> tokens;
    while (true) {
        Token t = lexer_next_token(&lexer);
        tokens.push_back(t);
        if (t.type == TOKEN_EOF || t.type == TOKEN_ERROR) {
            break;
        }
    }
    return tokens;
}

// Helper macro to assert token type and value.
#define EXPECT_TOKEN(tok, expected_type, expected_value) \
    EXPECT_EQ((tok).type, expected_type);                \
    if (expected_value) EXPECT_STREQ((tok).value, expected_value);

// Test basic tokenization of a simple statement
TEST(LexerTest, BasicTokenization) {
    const char* source = "var x = 42;";
    auto tokens = tokenizeSource(source);

    ASSERT_GE(tokens.size(), 5u);
    EXPECT_TOKEN(tokens[0], TOKEN_KEYWORD, "var");
    EXPECT_TOKEN(tokens[1], TOKEN_IDENTIFIER, "x");
    EXPECT_TOKEN(tokens[2], TOKEN_OPERATOR, "=");
    EXPECT_TOKEN(tokens[3], TOKEN_NUMBER, "42");
    EXPECT_TOKEN(tokens[4], TOKEN_PUNCTUATION, ";");
    EXPECT_EQ(tokens[5].type, TOKEN_EOF);

    // Clean up token values
    for (auto &t : tokens) free_token(&t);
}

// Test that keywords and identifiers are properly recognized
TEST(LexerTest, KeywordsAndIdentifiers) {
    const char* source = "function displayStats(name, hp, gp, items) { var isAlive = true; }";
    auto tokens = tokenizeSource(source);

    // Expected sequence: function (keyword), displayStats (identifier), ( (punct), name (id), , (punct), hp (id), , (punct), gp (id), , (punct), items (id), ) (punct), { (punct), var (kw), isAlive (id), = (op), true (boolean), ; (punct), } (punct)
    EXPECT_TOKEN(tokens[0], TOKEN_KEYWORD, "function");
    EXPECT_TOKEN(tokens[1], TOKEN_IDENTIFIER, "displayStats");
    EXPECT_TOKEN(tokens[2], TOKEN_PUNCTUATION, "(");
    EXPECT_TOKEN(tokens[3], TOKEN_IDENTIFIER, "name");
    EXPECT_TOKEN(tokens[4], TOKEN_PUNCTUATION, ",");
    EXPECT_TOKEN(tokens[5], TOKEN_IDENTIFIER, "hp");
    EXPECT_TOKEN(tokens[6], TOKEN_PUNCTUATION, ",");
    EXPECT_TOKEN(tokens[7], TOKEN_IDENTIFIER, "gp");
    EXPECT_TOKEN(tokens[8], TOKEN_PUNCTUATION, ",");
    EXPECT_TOKEN(tokens[9], TOKEN_IDENTIFIER, "items");
    EXPECT_TOKEN(tokens[10], TOKEN_PUNCTUATION, ")");
    EXPECT_TOKEN(tokens[11], TOKEN_PUNCTUATION, "{");
    EXPECT_TOKEN(tokens[12], TOKEN_KEYWORD, "var");
    EXPECT_TOKEN(tokens[13], TOKEN_IDENTIFIER, "isAlive");
    EXPECT_TOKEN(tokens[14], TOKEN_OPERATOR, "=");
    EXPECT_TOKEN(tokens[15], TOKEN_BOOLEAN, "true");
    EXPECT_TOKEN(tokens[16], TOKEN_PUNCTUATION, ";");
    EXPECT_TOKEN(tokens[17], TOKEN_PUNCTUATION, "}");
    EXPECT_EQ(tokens[18].type, TOKEN_EOF);

    for (auto &t : tokens) free_token(&t);
}

// Test string literals, including concatenation and special characters
TEST(LexerTest, StringLiterals) {
    const char* source = "var greeting = \"Hello, world!\"; var message = \"You\\nare\\tgreat!\";";
    auto tokens = tokenizeSource(source);

    // var greeting = "Hello, world!";
    EXPECT_TOKEN(tokens[0], TOKEN_KEYWORD, "var");
    EXPECT_TOKEN(tokens[1], TOKEN_IDENTIFIER, "greeting");
    EXPECT_TOKEN(tokens[2], TOKEN_OPERATOR, "=");
    EXPECT_TOKEN(tokens[3], TOKEN_STRING, "Hello, world!");
    EXPECT_TOKEN(tokens[4], TOKEN_PUNCTUATION, ";");

    // var message = "You\nare\tgreat!";
    EXPECT_TOKEN(tokens[5], TOKEN_KEYWORD, "var");
    EXPECT_TOKEN(tokens[6], TOKEN_IDENTIFIER, "message");
    EXPECT_TOKEN(tokens[7], TOKEN_OPERATOR, "=");
    // The lexer currently handles a few escape sequences. The actual stored string might contain '\n' and '\t'.
    // Expected: "You\nare\tgreat!"
    EXPECT_TOKEN(tokens[8], TOKEN_STRING, "You\nare\tgreat!");
    EXPECT_TOKEN(tokens[9], TOKEN_PUNCTUATION, ";");
    EXPECT_EQ(tokens[10].type, TOKEN_EOF);

    for (auto &t : tokens) free_token(&t);
}

// Test boolean, null, and keywords from example scripts
TEST(LexerTest, BooleanNullTokens) {
    const char* source = "if (isAlive == false) { return null; } else { return true; }";
    auto tokens = tokenizeSource(source);

    EXPECT_TOKEN(tokens[0], TOKEN_KEYWORD, "if");
    EXPECT_TOKEN(tokens[1], TOKEN_PUNCTUATION, "(");
    EXPECT_TOKEN(tokens[2], TOKEN_IDENTIFIER, "isAlive");
    EXPECT_TOKEN(tokens[3], TOKEN_OPERATOR, "==");
    EXPECT_TOKEN(tokens[4], TOKEN_BOOLEAN, "false");
    EXPECT_TOKEN(tokens[5], TOKEN_PUNCTUATION, ")");
    EXPECT_TOKEN(tokens[6], TOKEN_PUNCTUATION, "{");
    EXPECT_TOKEN(tokens[7], TOKEN_KEYWORD, "return");
    EXPECT_TOKEN(tokens[8], TOKEN_NULL, "null");
    EXPECT_TOKEN(tokens[9], TOKEN_PUNCTUATION, ";");
    EXPECT_TOKEN(tokens[10], TOKEN_PUNCTUATION, "}");
    EXPECT_TOKEN(tokens[11], TOKEN_KEYWORD, "else");
    EXPECT_TOKEN(tokens[12], TOKEN_PUNCTUATION, "{");
    EXPECT_TOKEN(tokens[13], TOKEN_KEYWORD, "return");
    EXPECT_TOKEN(tokens[14], TOKEN_BOOLEAN, "true");
    EXPECT_TOKEN(tokens[15], TOKEN_PUNCTUATION, ";");
    EXPECT_TOKEN(tokens[16], TOKEN_PUNCTUATION, "}");
    EXPECT_EQ(tokens[17].type, TOKEN_EOF);

    for (auto &t : tokens) free_token(&t);
}

// Test handling of whitespace and comments
TEST(LexerTest, WhitespaceAndComments) {
    const char* source =
        "var x = 10; // This is a comment\n"
        "/* A block comment\n"
        "   spanning multiple lines */\n"
        "x = x + 5;";

    auto tokens = tokenizeSource(source);

    // The comments should be skipped entirely
    // var x = 10;
    EXPECT_TOKEN(tokens[0], TOKEN_KEYWORD, "var");
    EXPECT_TOKEN(tokens[1], TOKEN_IDENTIFIER, "x");
    EXPECT_TOKEN(tokens[2], TOKEN_OPERATOR, "=");
    EXPECT_TOKEN(tokens[3], TOKEN_NUMBER, "10");
    EXPECT_TOKEN(tokens[4], TOKEN_PUNCTUATION, ";");

    // After comments, next line:
    // x = x + 5;
    EXPECT_TOKEN(tokens[5], TOKEN_IDENTIFIER, "x");
    EXPECT_TOKEN(tokens[6], TOKEN_OPERATOR, "=");
    EXPECT_TOKEN(tokens[7], TOKEN_IDENTIFIER, "x");
    EXPECT_TOKEN(tokens[8], TOKEN_OPERATOR, "+");
    EXPECT_TOKEN(tokens[9], TOKEN_NUMBER, "5");
    EXPECT_TOKEN(tokens[10], TOKEN_PUNCTUATION, ";");
    EXPECT_EQ(tokens[11].type, TOKEN_EOF);

    for (auto &t : tokens) free_token(&t);
}

// Test operators including logical and comparison operators
TEST(LexerTest, Operators) {
    const char* source = "if (x >= 10 && y <= 5 || z != 3) { z = x + y; }";
    auto tokens = tokenizeSource(source);

    EXPECT_TOKEN(tokens[0], TOKEN_KEYWORD, "if");
    EXPECT_TOKEN(tokens[1], TOKEN_PUNCTUATION, "(");
    EXPECT_TOKEN(tokens[2], TOKEN_IDENTIFIER, "x");
    EXPECT_TOKEN(tokens[3], TOKEN_OPERATOR, ">=");
    EXPECT_TOKEN(tokens[4], TOKEN_NUMBER, "10");
    EXPECT_TOKEN(tokens[5], TOKEN_OPERATOR, "&&");
    EXPECT_TOKEN(tokens[6], TOKEN_IDENTIFIER, "y");
    EXPECT_TOKEN(tokens[7], TOKEN_OPERATOR, "<=");
    EXPECT_TOKEN(tokens[8], TOKEN_NUMBER, "5");
    EXPECT_TOKEN(tokens[9], TOKEN_OPERATOR, "||");
    EXPECT_TOKEN(tokens[10], TOKEN_IDENTIFIER, "z");
    EXPECT_TOKEN(tokens[11], TOKEN_OPERATOR, "!=");
    EXPECT_TOKEN(tokens[12], TOKEN_NUMBER, "3");
    EXPECT_TOKEN(tokens[13], TOKEN_PUNCTUATION, ")");
    EXPECT_TOKEN(tokens[14], TOKEN_PUNCTUATION, "{");
    EXPECT_TOKEN(tokens[15], TOKEN_IDENTIFIER, "z");
    EXPECT_TOKEN(tokens[16], TOKEN_OPERATOR, "=");
    EXPECT_TOKEN(tokens[17], TOKEN_IDENTIFIER, "x");
    EXPECT_TOKEN(tokens[18], TOKEN_OPERATOR, "+");
    EXPECT_TOKEN(tokens[19], TOKEN_IDENTIFIER, "y");
    EXPECT_TOKEN(tokens[20], TOKEN_PUNCTUATION, ";");
    EXPECT_TOKEN(tokens[21], TOKEN_PUNCTUATION, "}");
    EXPECT_EQ(tokens[22].type, TOKEN_EOF);

    for (auto &t : tokens) free_token(&t);
}

// Optionally test unterminated strings (should produce TOKEN_ERROR)
TEST(LexerTest, UnterminatedString) {
    const char* source = "var str = \"Unfinished string";
    auto tokens = tokenizeSource(source);

    // var str = 
    EXPECT_TOKEN(tokens[0], TOKEN_KEYWORD, "var");
    EXPECT_TOKEN(tokens[1], TOKEN_IDENTIFIER, "str");
    EXPECT_TOKEN(tokens[2], TOKEN_OPERATOR, "=");

    // The next token should be TOKEN_ERROR due to unterminated string
    EXPECT_EQ(tokens[3].type, TOKEN_ERROR);

    for (auto &t : tokens) {
        free_token(&t);
    }
}

// Test from actual script snippet including concatenations and variables
TEST(LexerTest, FromAdventureGameSnippet) {
    const char* source =
        "var playerName = \"Adventurer\";\n"
        "var health = 50;\n"
        "var gold = 0;\n"
        "var inventory = \"Sword\";\n"
        "var isAlive = true;\n"
        "function displayStats(name, hp, gp, items) {\n"
        "   print(\"Player Stats:\");\n"
        "   print(\"Name: \" + name);\n"
        "}";

    auto tokens = tokenizeSource(source);

    // Check a few selected tokens:
    // var playerName = "Adventurer";
    EXPECT_TOKEN(tokens[0], TOKEN_KEYWORD, "var");
    EXPECT_TOKEN(tokens[1], TOKEN_IDENTIFIER, "playerName");
    EXPECT_TOKEN(tokens[2], TOKEN_OPERATOR, "=");
    EXPECT_TOKEN(tokens[3], TOKEN_STRING, "Adventurer");
    EXPECT_TOKEN(tokens[4], TOKEN_PUNCTUATION, ";");

    // var health = 50;
    EXPECT_TOKEN(tokens[5], TOKEN_KEYWORD, "var");
    EXPECT_TOKEN(tokens[6], TOKEN_IDENTIFIER, "health");
    EXPECT_TOKEN(tokens[7], TOKEN_OPERATOR, "=");
    EXPECT_TOKEN(tokens[8], TOKEN_NUMBER, "50");
    EXPECT_TOKEN(tokens[9], TOKEN_PUNCTUATION, ";");

    // var gold = 0;
    EXPECT_TOKEN(tokens[10], TOKEN_KEYWORD, "var");
    EXPECT_TOKEN(tokens[11], TOKEN_IDENTIFIER, "gold");
    EXPECT_TOKEN(tokens[12], TOKEN_OPERATOR, "=");
    EXPECT_TOKEN(tokens[13], TOKEN_NUMBER, "0");
    EXPECT_TOKEN(tokens[14], TOKEN_PUNCTUATION, ";");

    // var inventory = "Sword";
    EXPECT_TOKEN(tokens[15], TOKEN_KEYWORD, "var");
    EXPECT_TOKEN(tokens[16], TOKEN_IDENTIFIER, "inventory");
    EXPECT_TOKEN(tokens[17], TOKEN_OPERATOR, "=");
    EXPECT_TOKEN(tokens[18], TOKEN_STRING, "Sword");
    EXPECT_TOKEN(tokens[19], TOKEN_PUNCTUATION, ";");

    // var isAlive = true;
    EXPECT_TOKEN(tokens[20], TOKEN_KEYWORD, "var");
    EXPECT_TOKEN(tokens[21], TOKEN_IDENTIFIER, "isAlive");
    EXPECT_TOKEN(tokens[22], TOKEN_OPERATOR, "=");
    EXPECT_TOKEN(tokens[23], TOKEN_BOOLEAN, "true");
    EXPECT_TOKEN(tokens[24], TOKEN_PUNCTUATION, ";");

    // function displayStats(name, hp, gp, items) {
    EXPECT_TOKEN(tokens[25], TOKEN_KEYWORD, "function");
    EXPECT_TOKEN(tokens[26], TOKEN_IDENTIFIER, "displayStats");
    EXPECT_TOKEN(tokens[27], TOKEN_PUNCTUATION, "(");
    EXPECT_TOKEN(tokens[28], TOKEN_IDENTIFIER, "name");
    EXPECT_TOKEN(tokens[29], TOKEN_PUNCTUATION, ",");
    EXPECT_TOKEN(tokens[30], TOKEN_IDENTIFIER, "hp");
    EXPECT_TOKEN(tokens[31], TOKEN_PUNCTUATION, ",");
    EXPECT_TOKEN(tokens[32], TOKEN_IDENTIFIER, "gp");
    EXPECT_TOKEN(tokens[33], TOKEN_PUNCTUATION, ",");
    EXPECT_TOKEN(tokens[34], TOKEN_IDENTIFIER, "items");
    EXPECT_TOKEN(tokens[35], TOKEN_PUNCTUATION, ")");
    EXPECT_TOKEN(tokens[36], TOKEN_PUNCTUATION, "{");

    // print("Player Stats:");
    EXPECT_TOKEN(tokens[37], TOKEN_IDENTIFIER, "print");
    EXPECT_TOKEN(tokens[38], TOKEN_PUNCTUATION, "(");
    EXPECT_TOKEN(tokens[39], TOKEN_STRING, "Player Stats:");
    EXPECT_TOKEN(tokens[40], TOKEN_PUNCTUATION, ")");
    EXPECT_TOKEN(tokens[41], TOKEN_PUNCTUATION, ";");

    // print("Name: " + name);
    EXPECT_TOKEN(tokens[42], TOKEN_IDENTIFIER, "print");
    EXPECT_TOKEN(tokens[43], TOKEN_PUNCTUATION, "(");
    EXPECT_TOKEN(tokens[44], TOKEN_STRING, "Name: ");
    EXPECT_TOKEN(tokens[45], TOKEN_OPERATOR, "+");
    EXPECT_TOKEN(tokens[46], TOKEN_IDENTIFIER, "name");
    EXPECT_TOKEN(tokens[47], TOKEN_PUNCTUATION, ")");
    EXPECT_TOKEN(tokens[48], TOKEN_PUNCTUATION, ";");

    // }
    EXPECT_TOKEN(tokens[49], TOKEN_PUNCTUATION, "}");
    EXPECT_EQ(tokens[50].type, TOKEN_EOF);

    for (auto &t : tokens) free_token(&t);
}
