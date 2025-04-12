# Direct Nested Property Support in EmberScript VM

This document describes the implementation of direct nested property assignment support in the EmberScript Virtual Machine.

## Overview

EmberScript now offers direct nested property assignment in the VM mode, allowing expressions like `obj.x.y = value` to work seamlessly without requiring workarounds. This aligns the VM behavior with the AST interpreter, providing a more intuitive and consistent programming experience.

## Implementation Details

### New Opcode

A dedicated bytecode instruction has been added to handle nested property assignments:

```
OP_SET_NESTED_PROPERTY  // For nested property paths (obj.x.y = value)
```

This opcode operates on three stack values:
1. The base object to modify
2. A string representing the full property path (e.g., "x.y.z")
3. The value to assign to the nested property

### Recursive Property Path Handling

The VM implements a recursive function `vm_set_nested_property` that:
- Parses the property path string (e.g., "x.y.z")
- Traverses the object structure one level at a time
- Creates intermediate objects as needed
- Sets the final property value at the target location

### Compiler Support

The compiler has been enhanced to:
- Detect nested property access patterns in the AST
- Build the complete property path as a string
- Generate the appropriate bytecode using `OP_SET_NESTED_PROPERTY`

## Benefits

1. **Simpler Code**: No need for the workaround pattern of extracting, modifying, and reassigning objects.
2. **Improved Performance**: Direct nested property assignments avoid multiple property lookups and variable storage.
3. **Automatic Object Creation**: Missing intermediate objects along the property path are automatically created.
4. **Full Path Support**: Works with arbitrarily deep nested property paths.

## Usage Example

Before:
```javascript
// Workaround pattern
var inner = obj.x;
inner.y = "new value";
obj.x = inner;
```

After:
```javascript
// Direct assignment
obj.x.y = "new value";
```

## Testing

The implementation has been tested with cases covering:
- Basic nested property access (`obj.x.y`)
- Deep nesting (`obj.a.b.c.d`)
- Creating missing intermediate objects
- Various data types as values
- Edge cases like non-object properties

## Compatibility

This feature maintains full backward compatibility:
- The previous workaround pattern still works
- The AST interpreter behavior remains unchanged
- Existing code will benefit automatically when running in VM mode

## Limitations

- The property path is built at compile time, so dynamic property paths (using variables) are not supported
- Very deep nesting (more than a few dozen levels) may cause performance issues 