# EmberScript Event System - Current Status & Implementation Plan

## Table of Contents
- [Current Status](#current-status)
- [Critical Issues](#critical-issues)
- [What's Working](#whats-working)
- [What's Broken](#whats-broken)
- [Detailed Implementation Plan](#detailed-implementation-plan)
- [Code Structure Overview](#code-structure-overview)
- [Testing Strategy](#testing-strategy)

## Current Status

**Overall Progress:** ~25% complete (Infrastructure in place, core functionality broken)

**Last Working State:** Event firing (`fire["EventName"]`) parses and compiles successfully with placeholder output. Event binding (`fn() <- ["EventName"]`) fails to parse.

**Test Results:**
- ✅ `fire["TestEvent"]` - Parses and runs
- ❌ `onPlayerJump: fn() <- ["PlayerJump"]` - Parse error: "Expected '{' or indented block for function body"

## Critical Issues

### 1. **Event Binding Parsing Failure** (BLOCKING)
**Problem:** The parser cannot handle the event binding syntax `fn() <- ["EventName"]`

**Root Cause:** When parsing `onPlayerJump: fn() <- ["PlayerJump"]`:
1. Parser sees `onPlayerJump:` and calls `parse_function_definition()`
2. `parse_function_definition()` expects `fn(params) { body }` pattern
3. Parser encounters `<-` operator and fails because it's not expected in function definition context
4. Error: "Expected '{' or indented block for function body"

**Location:** `src/parser.c:1154` - `parse_function_definition()` function

**Solution Required:** Modify parsing logic to detect `fn() <-` pattern and route to `parse_event_binding()` instead.

### 2. **Missing Event Binding Detection Logic**
**Problem:** No logic exists to distinguish between:
- Regular function: `myFunc: fn() { ... }`
- Event binding: `myFunc: fn() <- ["EventName"] { ... }`

**Solution Required:** Add lookahead logic to detect `<-` after `fn()` and route appropriately.

### 3. **Incomplete Runtime Implementation**
**Problem:** All event functionality is placeholder text output only.
- No actual event listener registration
- No actual event firing mechanism
- No connection between parsed events and runtime execution

## What's Working

### ✅ Infrastructure Complete
1. **Lexer Support:**
   - `<-` operator tokenized correctly
   - `fire` keyword recognized
   - Event syntax tokens parsed

2. **AST Node Types:**
   - `AST_EVENT_BINDING` - Event listener definition
   - `AST_EVENT_BROADCAST` - Event firing
   - `AST_EVENT_CONDITION` - Conditional event execution
   - `AST_EVENT_FILTER` - Event filtering
   - `AST_FILTER_EXPRESSION` - Filter expressions

3. **Data Structures:**
   - `EventListener` struct defined
   - `EventRegistry` struct defined
   - `EventData` struct defined
   - `Filter` and `FilterType` enums defined

4. **Compiler Integration:**
   - Event nodes handled without infinite recursion
   - Placeholder compilation output works
   - Build system includes event_system.c

5. **Event Broadcasting:**
   - `fire["EventName"]` parses correctly
   - `parse_event_broadcast()` function implemented
   - AST creation for broadcast events works

### ✅ Test Infrastructure
- Test scripts can be created and run
- Build system works correctly
- Basic parsing verification works

## What's Broken

### ❌ Core Functionality
1. **Event Binding Parsing:** Cannot parse `fn() <- ["EventName"]` syntax
2. **Event Listener Registration:** No runtime registration of listeners
3. **Event Firing Execution:** Events don't actually trigger listeners
4. **Function Storage:** Event listener functions not stored for execution
5. **Condition Evaluation:** `{if condition}` syntax not implemented
6. **Filter Processing:** `| filter1, filter2` syntax not implemented

### ❌ Compiler Issues
1. **Event Compilation:** Only placeholder output, no bytecode generation
2. **Symbol Table Integration:** Event functions not properly registered
3. **Runtime Value Creation:** Event listeners not converted to callable functions

### ❌ VM Integration
1. **Event Operations:** Bytecode operations defined but not implemented
2. **Event Registry:** Global registry exists but unused
3. **Event Context:** No event parameter passing

## Detailed Implementation Plan

### Phase 1A: Fix Event Binding Parsing (CRITICAL - Must Complete First)

#### Step 1: Modify Function Definition Parsing
**File:** `src/parser.c`
**Function:** `parse_function_definition()` (line ~1154)

**Current Code Pattern:**
```c
// name: fn(params) { body }
ASTNode* parse_function_definition(Parser* parser) {
    // ... get name and ':'
    // Expect 'fn' keyword
    if (!match_token(parser, TOKEN_KEYWORD, "fn")) {
        // error
    }
    // Expect '(' 
    // ... parse params
    // Expect '{' or indented block  <-- FAILS HERE when encountering '<-'
}
```

**Required Change:** Add lookahead logic after parsing `fn()` to check for `<-`:
```c
ASTNode* parse_function_definition(Parser* parser) {
    // ... parse name, ':', 'fn', params
    
    // NEW: Check for event binding syntax
    if (parser->current_token.type == TOKEN_OPERATOR && 
        strcmp(parser->current_token.value, "<-") == 0) {
        // This is an event binding, not a regular function
        return parse_event_binding_from_function_start(parser, function_name, params);
    }
    
    // Regular function definition continues...
    // Expect '{' or indented block
}
```

#### Step 2: Implement Event Binding Parser Function
**File:** `src/parser.c`
**Function:** `parse_event_binding_from_function_start()` (NEW)

```c
ASTNode* parse_event_binding_from_function_start(Parser* parser, char* function_name, char** params, int param_count) {
    // Expect '<-' (already verified)
    parser_advance(parser);
    
    // Expect '['
    if (!match_token(parser, TOKEN_PUNCTUATION, "[")) {
        report_error(parser, "Expected '[' after '<-' in event binding");
        return NULL;
    }
    
    // Parse event name (string literal)
    if (parser->current_token.type != TOKEN_STRING) {
        report_error(parser, "Expected event name as string literal");
        return NULL;
    }
    
    char* event_name = strdup(parser->current_token.value);
    parser_advance(parser);
    
    // TODO: Parse optional condition {if condition}
    // TODO: Parse optional filters | filter1, filter2
    
    // Expect ']'
    if (!match_token(parser, TOKEN_PUNCTUATION, "]")) {
        report_error(parser, "Expected ']' to close event specification");
        return NULL;
    }
    
    // Parse function body (indented block or braces)
    ASTNode* body = NULL;
    if (parser->current_token.type == TOKEN_INDENT) {
        body = parse_indented_block(parser);
    } else if (parser->current_token.type == TOKEN_PUNCTUATION && 
               strcmp(parser->current_token.value, "{") == 0) {
        body = parse_block(parser);
    } else {
        report_error(parser, "Expected function body after event binding");
        return NULL;
    }
    
    // Create AST node
    ASTNode* node = create_ast_node(AST_EVENT_BINDING);
    node->event_binding.function_name = function_name;
    node->event_binding.parameters = params;
    node->event_binding.parameter_count = param_count;
    node->event_binding.event_name = event_name;
    node->event_binding.condition = NULL; // TODO: implement conditions
    node->event_binding.filter = NULL;    // TODO: implement filters
    node->event_binding.body = body;
    
    return node;
}
```

#### Step 3: Update parse_event_binding() Function
**File:** `src/parser.c`
**Function:** `parse_event_binding()` (already exists but incomplete)

Current function should be updated to handle the complete parsing or removed if using the approach above.

#### Step 4: Test Event Binding Parsing
**Test Script:** `test_scripts/test_event_binding_parsing.ember`
```ember
print("Testing event binding parsing...")

// Simple event binding
onTest: fn() <- ["TestEvent"]
  print("Test event fired!")

print("Event binding parsed successfully!")
```

### Phase 1B: Implement Basic Event Registration

#### Step 5: Implement Event Listener Registration
**File:** `src/compiler.c`
**Function:** Update `compile_statement()` for `AST_EVENT_BINDING`

**Current Code:**
```c
case AST_EVENT_BINDING:
    printf("Phase 1: Event node type %d (placeholder)\n", node->type);
    break;
```

**Required Implementation:**
```c
case AST_EVENT_BINDING: {
    // 1. Compile the function body into a separate chunk
    BytecodeChunk* event_chunk = vm_chunk_create();
    compile_function_body(node->event_binding.body, event_chunk, symtab);
    
    // 2. Create a user-defined function for the event handler
    RuntimeValue handler_func;
    handler_func.type = RUNTIME_VALUE_FUNCTION;
    handler_func.function_value.function_type = FUNCTION_TYPE_USER;
    
    UserDefinedFunction* user_func = malloc(sizeof(UserDefinedFunction));
    user_func->name = strdup(node->event_binding.function_name);
    user_func->parameter_count = node->event_binding.parameter_count;
    user_func->parameters = node->event_binding.parameters; // Copy as needed
    user_func->body = node->event_binding.body; // Share AST for now
    handler_func.function_value.user_function = user_func;
    
    // 3. Register the event listener with the event system
    event_register_listener(node->event_binding.event_name, 
                           node->event_binding.condition,
                           node->event_binding.filter,
                           handler_func);
    
    break;
}
```

#### Step 6: Implement Event System Registration Function
**File:** `src/event_system.c`
**Function:** `event_register_listener()` (needs implementation)

```c
void event_register_listener(const char* event_name, ASTNode* condition, 
                            ASTNode* filter, RuntimeValue handler_function) {
    // Initialize global registry if needed
    if (global_registry == NULL) {
        event_system_init();
    }
    
    // Create new event listener
    EventListener* listener = malloc(sizeof(EventListener));
    listener->event_name = strdup(event_name);
    listener->condition = condition;
    listener->filters = NULL; // TODO: implement filter parsing
    listener->function_body = handler_function.function_value.user_function->body;
    listener->owner_object = NULL; // TODO: track owner
    listener->priority = 0; // Default priority
    listener->next = NULL;
    
    // Add to hash table
    unsigned int hash = hash_string(event_name);
    unsigned int bucket = hash % global_registry->bucket_count;
    
    // Add to beginning of bucket chain
    listener->next = global_registry->buckets[bucket];
    global_registry->buckets[bucket] = listener;
    global_registry->total_listeners++;
    
    printf("DEBUG: Registered event listener for '%s'\n", event_name);
}
```

### Phase 1C: Implement Basic Event Firing

#### Step 7: Implement Event Firing
**File:** `src/compiler.c`
**Function:** Update `compile_statement()` for `AST_EVENT_BROADCAST`

**Current Code:**
```c
case AST_EVENT_BROADCAST:
    printf("Phase 1: Event node type %d (placeholder)\n", node->type);
    break;
```

**Required Implementation:**
```c
case AST_EVENT_BROADCAST: {
    // Emit bytecode to fire event at runtime
    // 1. Push event name as constant
    RuntimeValue event_name_val;
    event_name_val.type = RUNTIME_VALUE_STRING;
    event_name_val.string_value = strdup(node->event_broadcast.event_name);
    emit_constant(chunk, event_name_val);
    
    // 2. Emit OP_EVENT_FIRE instruction
    emit_byte(chunk, OP_EVENT_FIRE);
    
    break;
}
```

#### Step 8: Implement VM Event Fire Operation
**File:** `src/virtual_machine.c`
**Function:** Add case for `OP_EVENT_FIRE` in VM execution loop

```c
case OP_EVENT_FIRE: {
    // Pop event name from stack
    RuntimeValue event_name = vm_stack_pop(vm);
    
    if (event_name.type != RUNTIME_VALUE_STRING) {
        fprintf(stderr, "VM Error: Event name must be a string\n");
        return false;
    }
    
    // Fire the event
    event_fire_simple(event_name.string_value, vm);
    
    break;
}
```

#### Step 9: Implement Simple Event Firing Function
**File:** `src/event_system.c`
**Function:** `event_fire_simple()` (NEW)

```c
void event_fire_simple(const char* event_name, VM* vm) {
    if (global_registry == NULL) {
        printf("DEBUG: No event registry - event '%s' ignored\n", event_name);
        return;
    }
    
    // Find listeners for this event
    unsigned int hash = hash_string(event_name);
    unsigned int bucket = hash % global_registry->bucket_count;
    EventListener* listener = global_registry->buckets[bucket];
    
    int listener_count = 0;
    while (listener != NULL) {
        if (strcmp(listener->event_name, event_name) == 0) {
            listener_count++;
            
            // TODO: Evaluate condition if present
            // TODO: Apply filters if present
            
            // Execute the listener function
            printf("DEBUG: Executing listener %d for event '%s'\n", listener_count, event_name);
            
            // For Phase 1: Simple AST execution
            // TODO: In later phases, compile to bytecode and execute properly
            execute_event_listener_ast(listener, vm);
        }
        listener = listener->next;
    }
    
    if (listener_count == 0) {
        printf("DEBUG: No listeners found for event '%s'\n", event_name);
    } else {
        printf("DEBUG: Fired event '%s' to %d listeners\n", event_name, listener_count);
    }
}
```

#### Step 10: Implement Simple AST Execution for Event Listeners
**File:** `src/event_system.c`
**Function:** `execute_event_listener_ast()` (NEW)

```c
void execute_event_listener_ast(EventListener* listener, VM* vm) {
    // For Phase 1: Simple print statement execution
    // This is a minimal implementation to get events working
    
    if (listener->function_body && listener->function_body->type == AST_BLOCK) {
        // Execute each statement in the block
        for (int i = 0; i < listener->function_body->block.statement_count; i++) {
            ASTNode* stmt = listener->function_body->block.statements[i];
            
            // For Phase 1: Only handle print statements
            if (stmt->type == AST_FUNCTION_CALL && 
                strcmp(stmt->function_call.function_name, "print") == 0) {
                
                // Execute the print statement
                if (stmt->function_call.argument_count > 0) {
                    ASTNode* arg = stmt->function_call.arguments[0];
                    if (arg->type == AST_LITERAL && arg->literal.token_type == TOKEN_STRING) {
                        printf("%s\n", arg->literal.value);
                    }
                }
            }
        }
    }
}
```

### Phase 1D: Integration Testing

#### Step 11: Create Comprehensive Test
**File:** `test_scripts/test_phase1_complete.ember`
```ember
print("=== Phase 1 Complete Event System Test ===")

// Test 1: Register event listener
print("Registering event listener...")
onPlayerJump: fn() <- ["PlayerJump"]
  print("Player jumped!")

print("Event listener registered")

// Test 2: Fire the event
print("Firing PlayerJump event...")
fire["PlayerJump"]

print("Event system test complete!")
```

#### Step 12: Expected Output
```
=== Phase 1 Complete Event System Test ===
Registering event listener...
DEBUG: Registered event listener for 'PlayerJump'
Event listener registered
Firing PlayerJump event...
DEBUG: Executing listener 1 for event 'PlayerJump'
Player jumped!
DEBUG: Fired event 'PlayerJump' to 1 listeners
Event system test complete!
```

## Phase 2: Conditions & Basic Filters

### Step 13: Implement Condition Parsing
**File:** `src/parser.c`
**Function:** Extend event binding parser to handle `{if condition}`

### Step 14: Implement Condition Evaluation
**File:** `src/event_system.c`
**Function:** Add condition evaluation to `event_fire_simple()`

### Step 15: Implement Basic Filters
**File:** `src/parser.c` and `src/event_system.c`
**Functions:** Parse and evaluate `| all`, `| type(player)`, etc.

## Phase 3: Advanced Features

### Step 16: Custom Property Filters
### Step 17: Spatial Filters  
### Step 18: Priority System
### Step 19: Performance Optimization

## Code Structure Overview

### Key Files and Functions

#### Parser (`src/parser.c`)
- `parse_function_definition()` - **NEEDS MODIFICATION**
- `parse_event_binding()` - **NEEDS COMPLETION**
- `parse_event_broadcast()` - ✅ **WORKING**

#### Compiler (`src/compiler.c`)
- `compile_statement()` cases for event types - **NEEDS IMPLEMENTATION**
- Event bytecode generation - **NEEDS IMPLEMENTATION**

#### Event System (`src/event_system.c`)
- `event_register_listener()` - **NEEDS IMPLEMENTATION**
- `event_fire_simple()` - **NEEDS IMPLEMENTATION**
- `execute_event_listener_ast()` - **NEEDS IMPLEMENTATION**

#### Virtual Machine (`src/virtual_machine.c`)
- `OP_EVENT_FIRE` case - **NEEDS IMPLEMENTATION**
- Event operations integration - **NEEDS IMPLEMENTATION**

#### Headers
- `include/event_system.h` - ✅ **DATA STRUCTURES DEFINED**
- `include/parser.h` - ✅ **AST NODES DEFINED**
- `include/virtual_machine.h` - ✅ **OPCODES DEFINED**

## Testing Strategy

### Phase 1 Tests
1. **Event Binding Parsing Test:** Verify `fn() <- ["EventName"]` parses correctly
2. **Event Registration Test:** Verify listeners are stored in registry
3. **Event Firing Test:** Verify events trigger registered listeners
4. **Integration Test:** Complete end-to-end event flow

### Phase 2 Tests
1. **Condition Test:** Events with `{if condition}` syntax
2. **Filter Test:** Events with `| filter` syntax
3. **Complex Event Test:** Multiple listeners, conditions, and filters

### Phase 3 Tests
1. **Performance Test:** Large numbers of events and listeners
2. **Advanced Filter Test:** Custom property and spatial filters
3. **Game Scenario Test:** Realistic game development scenarios

## Critical Success Criteria

### Phase 1 Complete When:
1. ✅ `onPlayerJump: fn() <- ["PlayerJump"]` parses without error
2. ✅ `fire["PlayerJump"]` triggers the `onPlayerJump` function
3. ✅ Multiple event listeners can be registered for the same event
4. ✅ Event listeners execute and produce expected output
5. ✅ All existing functionality (iterators, functions, etc.) still works

### Implementation Priority
1. **CRITICAL:** Fix event binding parsing (blocking all other work)
2. **HIGH:** Implement basic event registration and firing
3. **MEDIUM:** Add condition and filter support
4. **LOW:** Performance optimization and advanced features

## Current Build Status
- ✅ Compiles without errors
- ✅ All existing tests pass
- ❌ Event binding test fails with parse error
- ✅ Event firing test passes (placeholder output)

## Next Steps for New Developer

1. **Start with:** Fix event binding parsing in `parse_function_definition()`
2. **Then:** Implement event listener registration in compiler
3. **Then:** Implement event firing mechanism in VM
4. **Test:** Verify complete Phase 1 functionality
5. **Continue:** Move to Phase 2 (conditions and filters)

This document provides a complete roadmap for continuing the EmberScript event system implementation from the current state to full functionality. 