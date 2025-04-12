# Nested Property Assignment in EmberScript

This document explains the implementation of direct nested property assignment support in EmberScript, allowing scripts to directly assign values to nested properties.

## Overview

EmberScript now supports direct nested property assignments with syntax such as:

```
obj.x.y.z = value;
```

This enables more intuitive code when working with deeply nested objects, eliminating the need for intermediate variables or manual property traversal.

## Implementation Details

### New VM Instruction

A dedicated bytecode instruction `OP_SET_NESTED_PROPERTY` was added to handle nested property assignments efficiently. This instruction:

1. Takes three stack values:
   - The base object to modify
   - A string representing the full property path (e.g., "x.y.z")
   - The value to assign

2. Recursively traverses the property path to set the value at the correct depth

### Compiler Changes

The compiler was enhanced to detect nested property assignments in the AST and generate the appropriate bytecode:

1. When it encounters a property assignment where the left-hand side is itself a property access (e.g., `obj.x.y = value`), it extracts the full property path.
2. The path is built by traversing the property access chain.
3. The compiler then emits the `OP_SET_NESTED_PROPERTY` instruction with the base object, property path, and value.

### VM Implementation

The implementation includes:

1. A recursive function to handle arbitrarily deep property paths
2. Property path parsing and traversal
3. Automatic creation of intermediate objects if they don't exist
4. Proper deep copying of objects to ensure immutability

### Global Variable Management

Special care is taken to ensure that:

1. Objects stored in global variables are properly updated after modification
2. Deep copies are made to prevent unintended shared references
3. Memory is properly managed to prevent leaks

## Usage Example

```javascript
// Create a nested object
var obj = {};
obj.user = {};
obj.user.profile = {};

// Direct nested property assignment
obj.user.profile.name = "John";
obj.user.profile.age = 30;

// Reading nested properties
print(obj.user.profile.name);  // Output: John
```

## Benefits

1. **Cleaner code**: No need for multiple variable assignments
2. **Better readability**: Property traversal is explicit and clear
3. **Reduced errors**: Less risk of mistakes in property traversal
4. **Performance**: Optimized implementation for nested property operations

## Limitations

1. This implementation currently handles dot notation access only (not bracket notation)
2. All intermediate properties must be objects, not other types
3. The maximum nesting depth is limited only by stack space 