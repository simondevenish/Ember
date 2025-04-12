# EmberScript Object Properties Guide

## Basic Object Usage

EmberScript supports JavaScript-like object literals and property access:

```js
// Create an object
var person = {
    name: "John",
    age: 30
};

// Access properties
print(person.name);  // John
print(person.age);   // 30

// Modify properties
person.age = 31;
print(person.age);   // 31

// Add new properties
person.city = "New York";
print(person.city);  // New York
```

## Nested Objects

You can create nested objects with multiple levels:

```js
var company = {
    name: "TechCorp",
    headquarters: {
        city: "San Francisco",
        country: "USA"
    },
    employees: 500
};

// Access nested properties
print(company.name);                   // TechCorp
print(company.headquarters.city);      // San Francisco
print(company.headquarters.country);   // USA
```

## Nested Property Assignment

### AST Interpreter Mode

When running EmberScript in AST interpreter mode (using `test_runner`), you can directly assign to nested properties:

```js
// This works in AST interpreter mode
company.headquarters.city = "Los Angeles";
print(company.headquarters.city);  // Los Angeles
```

### VM Mode

When running in VM mode (using `emberc compile` and `emberc run`), there's a current limitation with directly assigning to nested properties. Instead, use this workaround pattern:

```js
// Get the inner object
var inner = company.headquarters;

// Modify the inner object
inner.city = "Los Angeles";

// Set the inner object back to the parent
company.headquarters = inner;

// Now the change is properly reflected
print(company.headquarters.city);  // Los Angeles
```

## Why the Workaround is Needed

The EmberScript virtual machine currently handles property access and assignment differently than the AST interpreter:

1. **AST Interpreter**: Has direct access to the full property path and can recursively resolve and update nested properties.

2. **VM Implementation**: Uses a stack-based approach where each element (object, property name, value) is handled separately, making nested property paths more challenging to process.

## Performance Considerations

The workaround approach is actually quite efficient:

1. No deep copying happens - objects are passed by reference
2. Using temporary variables makes the intention clear
3. It works consistently across both VM and AST interpreter modes

## Best Practices

1. For simple property access (`obj.prop`), use direct dot notation
2. For nested property assignment, use the temporary variable pattern
3. Consider structuring your code to minimize deep nesting of objects

## Future Improvements

Future versions of EmberScript may implement direct nested property assignment in VM mode, but the current workaround provides a reliable solution that works in all execution modes. 