#include "event_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global event registry
static EventRegistry* global_registry = NULL;

// Global event context
EventData* current_event = NULL;

// Hash function for event names
static unsigned int hash_string(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

void event_system_init(void) {
    if (global_registry != NULL) {
        return; // Already initialized
    }
    
    global_registry = malloc(sizeof(EventRegistry));
    global_registry->bucket_count = 64; // Start with 64 buckets
    global_registry->buckets = calloc(global_registry->bucket_count, sizeof(EventListener*));
    global_registry->total_listeners = 0;
    
    printf("Event system initialized with %d buckets\n", global_registry->bucket_count);
}

void event_system_cleanup(void) {
    if (global_registry == NULL) {
        return;
    }
    
    // Free all listeners
    for (int i = 0; i < global_registry->bucket_count; i++) {
        EventListener* listener = global_registry->buckets[i];
        while (listener != NULL) {
            EventListener* next = listener->next;
            free(listener->event_name);
            filter_free(listener->filters);
            free(listener);
            listener = next;
        }
    }
    
    free(global_registry->buckets);
    free(global_registry);
    global_registry = NULL;
    
    printf("Event system cleaned up\n");
}

void event_register_listener(const char* event_name, ASTNode* condition, 
                            Filter* filters, ASTNode* function_body, 
                            RuntimeValue* owner_object, int priority) {
    if (global_registry == NULL) {
        event_system_init();
    }
    
    // Create new listener
    EventListener* listener = malloc(sizeof(EventListener));
    listener->event_name = strdup(event_name);
    listener->condition = condition;
    listener->filters = filters;
    listener->function_body = function_body;
    listener->owner_object = owner_object;
    listener->priority = priority;
    listener->next = NULL;
    
    // Find appropriate bucket
    unsigned int hash = hash_string(event_name);
    int bucket_index = hash % global_registry->bucket_count;
    
    // Insert at beginning of bucket (simple insertion)
    listener->next = global_registry->buckets[bucket_index];
    global_registry->buckets[bucket_index] = listener;
    global_registry->total_listeners++;
    
    printf("Registered listener for event '%s' (bucket %d, priority %d)\n", 
           event_name, bucket_index, priority);
}

void event_fire(const char* event_name, ASTNode* condition, Filter* filters,
               RuntimeValue* parameters, int parameter_count, 
               RuntimeValue* source_object, VM* vm, SymbolTable* symtab) {
    if (global_registry == NULL) {
        printf("Warning: Firing event '%s' but event system not initialized\n", event_name);
        return;
    }
    
    printf("Firing event '%s' with %d parameters\n", event_name, parameter_count);
    
    // Create event data
    EventData event_data;
    event_data.event_name = strdup(event_name);
    event_data.parameters = parameters;
    event_data.parameter_count = parameter_count;
    event_data.source_object = source_object;
    event_data.timestamp = 0; // TODO: Add timestamp
    event_data.event_id = 0;   // TODO: Add unique ID generation
    
    // Set global current event
    EventData* previous_event = current_event;
    current_event = &event_data;
    
    // Find listeners for this event
    unsigned int hash = hash_string(event_name);
    int bucket_index = hash % global_registry->bucket_count;
    
    EventListener* listener = global_registry->buckets[bucket_index];
    int matched_listeners = 0;
    
    while (listener != NULL) {
        if (strcmp(listener->event_name, event_name) == 0) {
            // Check if listener's condition passes
            bool condition_passes = true;
            if (listener->condition != NULL) {
                // TODO: Evaluate condition AST node
                // For Phase 1, we'll assume conditions always pass
            }
            
            // Check if listener's filters match
            bool filters_match = true;
            if (listener->filters != NULL) {
                // TODO: Implement filter matching
                // For Phase 1, we'll assume filters always match
            }
            
            if (condition_passes && filters_match) {
                matched_listeners++;
                printf("  -> Executing listener %d for '%s'\n", matched_listeners, event_name);
                
                // TODO: Execute the listener's function body
                // For Phase 1, we'll just print a message
                printf("     [Listener execution placeholder]\n");
            }
        }
        listener = listener->next;
    }
    
    printf("Event '%s' matched %d listeners\n", event_name, matched_listeners);
    
    // Restore previous event context
    current_event = previous_event;
    free(event_data.event_name);
}

bool filter_matches(Filter* filter, RuntimeValue* object, EventData* event_data) {
    if (filter == NULL) {
        return true; // No filter means match all
    }
    
    switch (filter->type) {
        case FILTER_ALL:
            return true;
            
        case FILTER_TYPE:
            // TODO: Implement type checking
            return true;
            
        case FILTER_NAME:
            // TODO: Implement name checking
            return true;
            
        case FILTER_PROPERTY:
            // TODO: Implement property comparison
            return true;
            
        default:
            return true; // Default to match for Phase 1
    }
}

Filter* filter_create(FilterType type, const char* parameter, 
                     ComparisonOp comparison, RuntimeValue value) {
    Filter* filter = malloc(sizeof(Filter));
    filter->type = type;
    filter->parameter = parameter ? strdup(parameter) : NULL;
    filter->comparison = comparison;
    filter->value = value;
    filter->next = NULL;
    return filter;
}

void filter_free(Filter* filter) {
    while (filter != NULL) {
        Filter* next = filter->next;
        free(filter->parameter);
        filter = next;
    }
}

FilterType parse_filter_type(const char* filter_str) {
    if (strcmp(filter_str, "all") == 0) return FILTER_ALL;
    if (strcmp(filter_str, "type") == 0) return FILTER_TYPE;
    if (strcmp(filter_str, "role") == 0) return FILTER_ROLE;
    if (strcmp(filter_str, "name") == 0) return FILTER_NAME;
    if (strcmp(filter_str, "location") == 0) return FILTER_LOCATION;
    if (strcmp(filter_str, "near") == 0) return FILTER_NEAR;
    if (strcmp(filter_str, "priority") == 0) return FILTER_PRIORITY;
    if (strcmp(filter_str, "ui") == 0) return FILTER_UI;
    if (strcmp(filter_str, "debug") == 0) return FILTER_DEBUG;
    if (strcmp(filter_str, "target") == 0) return FILTER_TARGET;
    if (strcmp(filter_str, "owner") == 0) return FILTER_OWNER;
    return FILTER_PROPERTY; // Default to property filter
}

ComparisonOp parse_comparison_op(const char* op_str) {
    if (strcmp(op_str, "==") == 0) return COMP_EQUAL;
    if (strcmp(op_str, "!=") == 0) return COMP_NOT_EQUAL;
    if (strcmp(op_str, ">") == 0) return COMP_GREATER;
    if (strcmp(op_str, ">=") == 0) return COMP_GREATER_EQ;
    if (strcmp(op_str, "<") == 0) return COMP_LESS;
    if (strcmp(op_str, "<=") == 0) return COMP_LESS_EQ;
    return COMP_EQUAL; // Default
} 