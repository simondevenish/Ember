#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "compiler.h"
#include "virtual_machine.h"
#include "parser.h"
#include "lexer.h"
#include "runtime.h"
#include "interpreter.h"

// Forward declaration for usage printing:
static void print_usage(void);

/**
 * @brief Helper function to read a file's entire contents into a buffer (binary-safe).
 * 
 * @param filename The path to the file.
 * @return char*   Heap-allocated null-terminated string, or NULL on error.
 */
static char* read_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error: fseek() failed for file '%s'\n", filename);
        fclose(file);
        return NULL;
    }
    long length = ftell(file);
    if (length < 0) {
        fprintf(stderr, "Error: ftell() returned negative length for file '%s'\n", filename);
        fclose(file);
        return NULL;
    }
    rewind(file);

    char* buffer = (char*)malloc((size_t)length + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    size_t read_count = fread(buffer, 1, (size_t)length, file);
    buffer[read_count] = '\0';

    fclose(file);
    return buffer;
}

/**
 * @brief Read a serialized BytecodeChunk from a .embc file.
 *        Format: [int code_count], [int constants_count], code bytes, then constants.
 */
static BytecodeChunk* read_chunk(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open bytecode file '%s'\n", filename);
        return NULL;
    }

    BytecodeChunk* chunk = vm_create_chunk();
    if (!chunk) {
        fclose(file);
        return NULL;
    }

    // Read code_count
    if (fread(&chunk->code_count, sizeof(int), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read code_count from '%s'\n", filename);
        vm_free_chunk(chunk);
        fclose(file);
        return NULL;
    }
    // Read constants_count
    if (fread(&chunk->constants_count, sizeof(int), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read constants_count from '%s'\n", filename);
        vm_free_chunk(chunk);
        fclose(file);
        return NULL;
    }

    // Allocate code array
    chunk->code_capacity = chunk->code_count;
    chunk->code = (uint8_t*)malloc(chunk->code_count);
    if (!chunk->code) {
        fprintf(stderr, "Error: Memory allocation for code failed.\n");
        vm_free_chunk(chunk);
        fclose(file);
        return NULL;
    }
    // Read the code bytes
    if (fread(chunk->code, 1, chunk->code_count, file) != (size_t)chunk->code_count) {
        fprintf(stderr, "Error: Unable to read code bytes from '%s'\n", filename);
        vm_free_chunk(chunk);
        fclose(file);
        return NULL;
    }

    // Allocate constants array
    chunk->constants_capacity = chunk->constants_count;
    chunk->constants = (RuntimeValue*)malloc(chunk->constants_count * sizeof(RuntimeValue));
    if (!chunk->constants) {
        fprintf(stderr, "Error: Memory allocation for constants failed.\n");
        vm_free_chunk(chunk);
        fclose(file);
        return NULL;
    }

    // Read each constant's type and data
    for (int i = 0; i < chunk->constants_count; i++) {
        RuntimeValueType t;
        if (fread(&t, sizeof(RuntimeValueType), 1, file) != 1) {
            fprintf(stderr, "Error: Could not read constant type for index %d\n", i);
            vm_free_chunk(chunk);
            fclose(file);
            return NULL;
        }
        chunk->constants[i].type = t;

        switch (t) {
            case RUNTIME_VALUE_NUMBER: {
                double num;
                if (fread(&num, sizeof(double), 1, file) != 1) {
                    fprintf(stderr, "Error reading numeric constant.\n");
                    vm_free_chunk(chunk);
                    fclose(file);
                    return NULL;
                }
                chunk->constants[i].number_value = num;
            } break;

            case RUNTIME_VALUE_BOOLEAN: {
                bool bval;
                if (fread(&bval, sizeof(bool), 1, file) != 1) {
                    fprintf(stderr, "Error reading bool constant.\n");
                    vm_free_chunk(chunk);
                    fclose(file);
                    return NULL;
                }
                chunk->constants[i].boolean_value = bval;
            } break;

            case RUNTIME_VALUE_NULL: {
                // no extra data
            } break;

            case RUNTIME_VALUE_STRING: {
                int slen = 0;
                if (fread(&slen, sizeof(int), 1, file) != 1 || slen < 0) {
                    fprintf(stderr, "Error reading string length.\n");
                    vm_free_chunk(chunk);
                    fclose(file);
                    return NULL;
                }
                char* sdata = (char*)malloc(slen + 1);
                if (!sdata) {
                    fprintf(stderr, "Error allocating memory for string constant.\n");
                    vm_free_chunk(chunk);
                    fclose(file);
                    return NULL;
                }
                if (fread(sdata, 1, slen, file) != (size_t)slen) {
                    fprintf(stderr, "Error reading string constant data.\n");
                    free(sdata);
                    vm_free_chunk(chunk);
                    fclose(file);
                    return NULL;
                }
                sdata[slen] = '\0';
                chunk->constants[i].string_value = sdata;
            } break;

            case RUNTIME_VALUE_FUNCTION: {
                // For function constants, we only need to store the function type
                // and basic metadata. The actual function body is compiled separately.
                int func_type = (int)chunk->constants[i].function_value.function_type;
                fwrite(&func_type, sizeof(int), 1, file);
                
                if (chunk->constants[i].function_value.function_type == FUNCTION_TYPE_USER) {
                    // For user-defined functions, store the name and parameter count
                    UserDefinedFunction* user_func = chunk->constants[i].function_value.user_function;
                    if (user_func && user_func->name) {
                        int name_len = (int)strlen(user_func->name);
                        fwrite(&name_len, sizeof(int), 1, file);
                        fwrite(user_func->name, 1, name_len, file);
                        fwrite(&user_func->parameter_count, sizeof(int), 1, file);
                    } else {
                        // No name or invalid function
                        int name_len = 0;
                        int param_count = 0;
                        fwrite(&name_len, sizeof(int), 1, file);
                        fwrite(&param_count, sizeof(int), 1, file);
                    }
                }
                // For built-in functions, we don't need to store additional data
            } break;

            default:
                fprintf(stderr, "Error: Unsupported constant type %d in chunk.\n", (int)t);
                vm_free_chunk(chunk);
                fclose(file);
                return NULL;
        }
    }

    fclose(file);
    return chunk;
}

/**
 * @brief Write a BytecodeChunk to a .embc file.
 */
static int write_chunk(const char* filename, const BytecodeChunk* chunk) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Could not open output file '%s'\n", filename);
        return 1;
    }

    // Write code_count, constants_count
    fwrite(&chunk->code_count, sizeof(int), 1, file);
    fwrite(&chunk->constants_count, sizeof(int), 1, file);

    // Write code array
    fwrite(chunk->code, 1, chunk->code_count, file);

    // Write each constant's type + data
    for (int i = 0; i < chunk->constants_count; i++) {
        RuntimeValueType t = chunk->constants[i].type;
        fwrite(&t, sizeof(RuntimeValueType), 1, file);

        switch (t) {
            case RUNTIME_VALUE_NUMBER: {
                double num = chunk->constants[i].number_value;
                fwrite(&num, sizeof(double), 1, file);
            } break;

            case RUNTIME_VALUE_BOOLEAN: {
                bool bval = chunk->constants[i].boolean_value;
                fwrite(&bval, sizeof(bool), 1, file);
            } break;

            case RUNTIME_VALUE_NULL: {
                // no data
            } break;

            case RUNTIME_VALUE_STRING: {
                const char* s = chunk->constants[i].string_value ? chunk->constants[i].string_value : "";
                int slen = (int)strlen(s);
                fwrite(&slen, sizeof(int), 1, file);
                fwrite(s, 1, slen, file);
            } break;

            case RUNTIME_VALUE_FUNCTION: {
                // For function constants, we only need to store the function type
                // and basic metadata. The actual function body is compiled separately.
                int func_type = (int)chunk->constants[i].function_value.function_type;
                fwrite(&func_type, sizeof(int), 1, file);
                
                if (chunk->constants[i].function_value.function_type == FUNCTION_TYPE_USER) {
                    // For user-defined functions, store the name and parameter count
                    UserDefinedFunction* user_func = chunk->constants[i].function_value.user_function;
                    if (user_func && user_func->name) {
                        int name_len = (int)strlen(user_func->name);
                        fwrite(&name_len, sizeof(int), 1, file);
                        fwrite(user_func->name, 1, name_len, file);
                        fwrite(&user_func->parameter_count, sizeof(int), 1, file);
                    } else {
                        // No name or invalid function
                        int name_len = 0;
                        int param_count = 0;
                        fwrite(&name_len, sizeof(int), 1, file);
                        fwrite(&param_count, sizeof(int), 1, file);
                    }
                }
                // For built-in functions, we don't need to store additional data
            } break;

            default:
                fprintf(stderr, "Warning: Unknown constant type %d\n", (int)t);
                break;
        }
    }

    fclose(file);
    return 0; // success
}

/**
 * @brief Compile a .ember script into a BytecodeChunk in memory.
 */
static BytecodeChunk* compile_ember_source(const char* source) {
    // 1) Lex
    Lexer lexer;
    lexer_init(&lexer, source);

    // 2) Parse
    Parser* parser = parser_create(&lexer);
    ASTNode* root = parse_script(parser);
    if (!root) {
        fprintf(stderr, "Error: Parsing failed.\n");
        free(parser);
        return NULL;
    }

    // 3) Create BytecodeChunk
    BytecodeChunk* chunk = vm_create_chunk();
    if (!chunk) {
        free_ast(root);
        free(parser);
        return NULL;
    }

    // 4) Create SymbolTable
    SymbolTable* symtab = symbol_table_create();
    if (!symtab) {
        vm_free_chunk(chunk);
        free_ast(root);
        free(parser);
        return NULL;
    }

    // 5) Compile AST -> Bytecode
    bool ok = compile_ast(root, chunk, symtab);
    if (!ok) {
        fprintf(stderr, "Error: Compilation failed.\n");
        symbol_table_free(symtab);
        vm_free_chunk(chunk);
        free_ast(root);
        free(parser);
        return NULL;
    }

    symbol_table_free(symtab);
    free_ast(root);
    free(parser);

    return chunk;
}

/**
 * @brief Create a small C stub that includes the bytecode as an array, then
 *        compile that stub into a self-contained executable linked against libEmber.
 */
static int embed_chunk_in_exe(const char* outFile, const BytecodeChunk* chunk) {
    // 1) Write a temporary C file that embeds the chunk data
    FILE* stub = fopen("temp_stub.c", "w");
    if (!stub) {
        fprintf(stderr, "Error: Could not create temporary stub file.\n");
        return 1;
    }

    // We'll reference symbols from libEmber: vm_run, vm_create, vm_free, etc.
    // So we only embed the actual chunk data here.
    fprintf(stub, "#include <stdio.h>\n");
    fprintf(stub, "#include <stdlib.h>\n");
    fprintf(stub, "#include <string.h>\n");
    fprintf(stub, "#include \"virtual_machine.h\"\n");
    fprintf(stub, "#include \"runtime.h\"\n");
    fprintf(stub, "extern int vm_run(VM* vm);\n");
    fprintf(stub, "extern VM* vm_create(BytecodeChunk* chunk);\n");
    fprintf(stub, "extern void vm_free(VM* vm);\n");
    fprintf(stub, "extern void vm_free_chunk(BytecodeChunk* chunk);\n");

    // Embed the code array
    fprintf(stub, "static unsigned char code_data[%d] = {", chunk->code_count);
    for (int i = 0; i < chunk->code_count; i++) {
        fprintf(stub, "%u", (unsigned)chunk->code[i]);
        if (i < chunk->code_count - 1) {
            fprintf(stub, ",");
        }
    }
    fprintf(stub, "};\n");

    // Start main() and fill the BytecodeChunk structure
    fprintf(stub, "int main(void) {\n");
    fprintf(stub, "  BytecodeChunk chunk;\n");
    fprintf(stub, "  chunk.code_count = %d;\n", chunk->code_count);
    fprintf(stub, "  chunk.code_capacity = %d;\n", chunk->code_count);
    fprintf(stub, "  chunk.code = code_data;\n");
    fprintf(stub, "  chunk.constants_count = %d;\n", chunk->constants_count);
    fprintf(stub, "  chunk.constants_capacity = %d;\n", chunk->constants_count);
    fprintf(stub, "  chunk.constants = malloc(sizeof(RuntimeValue) * %d);\n", chunk->constants_count);
    fprintf(stub, "  if (!chunk.constants) {\n");
    fprintf(stub, "    fprintf(stderr, \"Failed to allocate constants.\\n\");\n");
    fprintf(stub, "    return 1;\n");
    fprintf(stub, "  }\n");

    // Emit code to embed constants
    for (int i = 0; i < chunk->constants_count; i++) {
        RuntimeValue val = chunk->constants[i];
        fprintf(stub, "  chunk.constants[%d].type = %d;\n", i, (int)val.type);

        switch (val.type) {
            case RUNTIME_VALUE_NUMBER:
                fprintf(stub, "  chunk.constants[%d].number_value = %f;\n", i, val.number_value);
                break;
            case RUNTIME_VALUE_BOOLEAN:
                fprintf(stub, "  chunk.constants[%d].boolean_value = %s;\n",
                        i, val.boolean_value ? "true" : "false");
                break;
            case RUNTIME_VALUE_NULL:
                // No additional data needed
                break;
            case RUNTIME_VALUE_STRING: {
                int slen = (int)strlen(val.string_value);
                fprintf(stub, "  {\n");
                fprintf(stub, "    static char s_%d[%d + 1] = {", i, slen);
                for (int c = 0; c < slen; c++) {
                    fprintf(stub, "%d", (unsigned char)val.string_value[c]);
                    if (c < slen - 1) {
                        fprintf(stub, ",");
                    }
                }
                fprintf(stub, "};\n");
                fprintf(stub, "    s_%d[%d] = '\\0';\n", i, slen);
                fprintf(stub, "    chunk.constants[%d].string_value = s_%d;\n", i, i);
                fprintf(stub, "  }\n");
            } break;
            case RUNTIME_VALUE_FUNCTION: {
                // For function constants, we only need to store the function type
                // and basic metadata. The actual function body is compiled separately.
                int func_type = (int)val.function_value.function_type;
                fprintf(stub, "  chunk.constants[%d].function_value.function_type = %d;\n",
                        i, func_type);
                
                if (val.function_value.function_type == FUNCTION_TYPE_USER) {
                    // For user-defined functions, store the name and parameter count
                    UserDefinedFunction* user_func = val.function_value.user_function;
                    if (user_func && user_func->name) {
                        int name_len = (int)strlen(user_func->name);
                        fprintf(stub, "  {\n");
                        fprintf(stub, "    static char s_%d[%d + 1] = {", i, name_len);
                        for (int c = 0; c < name_len; c++) {
                            fprintf(stub, "%d", (unsigned char)user_func->name[c]);
                            if (c < name_len - 1) {
                                fprintf(stub, ",");
                            }
                        }
                        fprintf(stub, "};\n");
                        fprintf(stub, "    s_%d[%d] = '\\0';\n", i, name_len);
                        fprintf(stub, "    chunk.constants[%d].function_value.user_function = &s_%d;\n",
                                i, i);
                        fprintf(stub, "    chunk.constants[%d].function_value.parameter_count = %d;\n",
                                i, user_func->parameter_count);
                        fprintf(stub, "  }\n");
                    } else {
                        // No name or invalid function
                        int name_len = 0;
                        int param_count = 0;
                        fprintf(stub, "  chunk.constants[%d].function_value.user_function = NULL;\n", i);
                        fprintf(stub, "  chunk.constants[%d].function_value.parameter_count = %d;\n",
                                i, param_count);
                    }
                }
                // For built-in functions, we don't need to store additional data
            } break;
            default:
                fprintf(stub, "  // Unknown constant type\n");
                break;
        }
    }

    // Create VM, run, cleanup
    fprintf(stub, "  VM* vm = vm_create(&chunk);\n");
    fprintf(stub, "  if (!vm) {\n");
    fprintf(stub, "    fprintf(stderr, \"Failed to create VM.\\n\");\n");
    fprintf(stub, "    free(chunk.constants);\n");
    fprintf(stub, "    return 1;\n");
    fprintf(stub, "  }\n");

    fprintf(stub, "  int r = vm_run(vm);\n");
    fprintf(stub, "  vm_free(vm);\n");
    fprintf(stub, "  free(chunk.constants);\n");
    fprintf(stub, "  return r;\n");
    fprintf(stub, "}\n");

    fclose(stub);

    // 2) Compile that stub into a self-contained executable linking with libEmber
    //    Adjust paths as needed for your environment:
    //    -I... for includes
    //    -L... for library path
    //    -lEmber for your static library name
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "cc temp_stub.c -o \"%s\" -I../include -L. -lEmber -lm -lpthread",
             outFile);

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "Error: system() compilation command failed with code %d\n", ret);
        return 1;
    }

    // Remove the stub if desired
    remove("temp_stub.c");

    return 0; // success
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char* subcommand = argv[1];
    const char* input_file = NULL;
    const char* output_file = NULL;

    // Parse optional "-o" for output
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && (i + 1 < argc)) {
            output_file = argv[i + 1];
            i++;
        } else {
            input_file = argv[i];
        }
    }

    // If subcommand is not recognized, treat it as the input file => "compile" by default
    if (strcmp(subcommand, "compile") != 0 && strcmp(subcommand, "run") != 0) {
        input_file = subcommand;
        subcommand = "compile";
    }

    if (!input_file) {
        fprintf(stderr, "Error: No input file specified.\n\n");
        print_usage();
        return 1;
    }

    // Subcommand "run" => read .embc, run in VM
    if (strcmp(subcommand, "run") == 0) {
        BytecodeChunk* chunk = read_chunk(input_file);
        if (!chunk) {
            return 1;
        }
        VM* vm = vm_create(chunk);
        if (!vm) {
            fprintf(stderr, "Error: Failed to create VM.\n");
            vm_free_chunk(chunk);
            return 1;
        }
        int status = vm_run(vm);
        vm_free(vm);
        vm_free_chunk(chunk);
        return status;
    } 
    // Otherwise "compile"
    else {
        if (!output_file) {
            output_file = "a.embc";
        }

        char* script_content = read_file(input_file);
        if (!script_content) {
            return 1;
        }

        // Compile the source code into a BytecodeChunk
        BytecodeChunk* chunk = compile_ember_source(script_content);
        free(script_content);
        if (!chunk) {
            return 1; // compile_ember_source already printed errors
        }

        // Decide if we produce an executable or a .embc file:
        //   - If there's no extension, or extension == ".exe", produce an executable
        //   - Otherwise, produce .embc
        const char* dot = strrchr(output_file, '.');
        bool isExe = false;

        if (!dot) {
            // No extension => default to an executable
            isExe = true;
        } else if (strcasecmp(dot, ".exe") == 0) {
            isExe = true;
        }

        if (isExe) {
            printf("Compiling '%s' => Executable '%s'\n", input_file, output_file);
            int ecode = embed_chunk_in_exe(output_file, chunk);
            vm_free_chunk(chunk);
            return ecode;
        } else {
            printf("Compiling '%s' => Bytecode '%s'\n", input_file, output_file);
            int ecode = write_chunk(output_file, chunk);
            vm_free_chunk(chunk);
            return ecode;
        }
    }

    // Not reached
    return 0;
}

static void print_usage(void) {
    printf(
        "Usage: emberc [subcommand] [input] [options]\n\n"
        "Subcommands:\n"
        "  compile (default)   - Compile a .ember file to either a native executable or .embc\n"
        "  run                  - Run a .embc bytecode file in the VM\n\n"
        "Logic for '-o':\n"
        "  - If you specify no extension, or use '.exe', emberc produces a native binary (linked against libEmber).\n"
        "  - Otherwise, emberc writes raw bytecode ('.embc').\n\n"
        "Examples:\n"
        "  emberc my_script.ember -o my_script       (produces native binary called 'my_script')\n"
        "  emberc my_script.ember -o my_script.exe   (produces native binary 'my_script.exe')\n"
        "  emberc run my_script.embc                 (runs existing bytecode)\n\n"
    );
}
