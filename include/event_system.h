#ifndef EVENT_SYSTEM_H
#define EVENT_SYSTEM_H

#include "parser.h"
#include "virtual_machine.h"
#include "compiler.h"
#include <stdbool.h>

// Filter types for event filtering
typedef enum {
    FILTER_ALL,               // | all
    FILTER_TYPE,              // | type(player)
    FILTER_ROLE,              // | role(guard)
    FILTER_NAME,              // | name(Hero)
    FILTER_PROPERTY,          // | health(>50)
    FILTER_LOCATION,          // | location(town)
    FILTER_NEAR,              // | near(object)
    FILTER_PRIORITY,          // | priority(high)
    FILTER_UI,                // | ui
    FILTER_DEBUG,             // | debug(true)
    FILTER_TARGET,            // | target(object)
    FILTER_OWNER              // | owner(object)
} FilterType;

// Comparison operators for property filters
typedef enum {
    COMP_EQUAL,      // ==
    COMP_NOT_EQUAL,  // !=
    COMP_GREATER,    // >
    COMP_GREATER_EQ, // >=
    COMP_LESS,       // <
    COMP_LESS_EQ     // <=
} ComparisonOp;

// Event filter structure
typedef struct Filter {
    FilterType type;           // Type of filter
    char* parameter;           // Filter parameter ("player", "health", etc.)
    ComparisonOp comparison;   // Comparison operator (for property filters)
    RuntimeValue value;        // Comparison value
    struct Filter* next;       // Next filter in chain
} Filter;

// Event listener structure
typedef struct EventListener {
    char* event_name;          // Name of the event to listen for
    ASTNode* condition;        // Optional condition block
    Filter* filters;           // Chain of filters
    ASTNode* function_body;    // Function body to execute
    RuntimeValue* owner_object; // Object that owns this listener
    int priority;              // Execution priority (0=low, 1=medium, 2=high)
    struct EventListener* next; // Next listener in hash bucket
} EventListener;

// Event registry (hash table)
typedef struct EventRegistry {
    EventListener** buckets;   // Array of hash buckets
    int bucket_count;          // Number of buckets
    int total_listeners;       // Total number of listeners
} EventRegistry;

// Event data passed to handlers
typedef struct EventData {
    char* event_name;          // Name of the event
    RuntimeValue* parameters;  // Event parameters (target, damage, etc.)
    int parameter_count;       // Number of parameters
    RuntimeValue* source_object; // Object that fired the event
    int timestamp;             // When the event was fired
    int event_id;              // Unique event ID
} EventData;

// Global event context for current event being processed
extern EventData* current_event;

// Event System API

/**
 * @brief Initialize the global event system
 */
void event_system_init(void);

/**
 * @brief Cleanup the global event system
 */
void event_system_cleanup(void);

/**
 * @brief Register an event listener
 * 
 * @param event_name Name of the event to listen for
 * @param condition Optional condition AST node
 * @param filters Filter chain for targeting
 * @param function_body Function body to execute
 * @param owner_object Object that owns this listener
 * @param priority Execution priority
 */
void event_register_listener(const char* event_name, ASTNode* condition, 
                            Filter* filters, ASTNode* function_body, 
                            RuntimeValue* owner_object, int priority);

/**
 * @brief Fire an event with given parameters
 * 
 * @param event_name Name of the event to fire
 * @param condition Optional condition AST node
 * @param filters Filter chain for targeting
 * @param parameters Event parameters
 * @param parameter_count Number of parameters
 * @param source_object Object that fired the event
 * @param vm Virtual machine instance for execution
 * @param symtab Symbol table for execution
 */
void event_fire(const char* event_name, ASTNode* condition, Filter* filters,
               RuntimeValue* parameters, int parameter_count, 
               RuntimeValue* source_object, VM* vm, SymbolTable* symtab);

/**
 * @brief Check if a filter matches an object
 * 
 * @param filter The filter to check
 * @param object The object to check against
 * @param event_data Current event data
 * @return true if the filter matches
 */
bool filter_matches(Filter* filter, RuntimeValue* object, EventData* event_data);

/**
 * @brief Create a new filter
 * 
 * @param type Filter type
 * @param parameter Filter parameter
 * @param comparison Comparison operator (for property filters)
 * @param value Comparison value
 * @return Filter* The new filter
 */
Filter* filter_create(FilterType type, const char* parameter, 
                     ComparisonOp comparison, RuntimeValue value);

/**
 * @brief Free a filter chain
 * 
 * @param filter The filter chain to free
 */
void filter_free(Filter* filter);

/**
 * @brief Parse filter type from string
 * 
 * @param filter_str The filter string (e.g., "type", "health")
 * @return FilterType The parsed filter type
 */
FilterType parse_filter_type(const char* filter_str);

/**
 * @brief Parse comparison operator from string
 * 
 * @param op_str The operator string (e.g., ">", "<=")
 * @return ComparisonOp The parsed comparison operator
 */
ComparisonOp parse_comparison_op(const char* op_str);

#endif // EVENT_SYSTEM_H 