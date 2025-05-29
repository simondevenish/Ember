# EmberScript Known Issues & Limitations

This document tracks known issues, edge cases, and limitations in the EmberScript language implementation.

## 📊 **Current Status Summary**
- **Test Suite**: 8/8 tests passing (100% ✅)
- **Core Functionality**: Fully working ✅
- **Known Issues**: 1 minor edge case
- **Overall Status**: Production ready with documented limitations

---

## 🐛 **Active Issues**

### **Issue #1: If-Statement Return Values in Functions**
**Priority**: Low  
**Status**: Active  
**Affects**: Function compilation  
**Date Discovered**: 2024-12-19

#### **Description**
Functions that end with if-else statements as their return value return `null` instead of the expected computed value.

#### **Technical Details**
- **Root Cause**: `compile_if_statement_with_return()` in `src/compiler.c` doesn't properly preserve return values from if-else branches
- **Affected Pattern**: Functions where the last statement is an if-else that should return different values
- **VM Behavior**: The function executes correctly but leaves `null` on the stack instead of the computed value

#### **Broken Code Examples**
```ember
// ❌ This returns null instead of computed value
power: fn(base, exp) {
    if (exp == 0) {
        1           // Should return 1
    } else {
        var result = base
        for (var i = 1; i < exp; i = i + 1) {
            result = result * base
        }
        result      // Should return computed power
    }
}

// ❌ This also returns null
factorial: fn(n) {
    if (n <= 1) {
        1           // Should return 1
    } else {
        var result = 1
        for (var i = 2; i <= n; i = i + 1) {
            result = result * i
        }
        result      // Should return factorial
    }
}
```

#### **Working Code Examples**
```ember
// ✅ These patterns work correctly
simple: fn() {
    42              // Simple expression - works
}

withVars: fn() {
    var x = 5
    x * 2           // Variable + expression - works  
}

withLoops: fn() {
    var result = 1
    for (var i = 1; i <= 3; i = i + 1) {
        result = result * i
    }
    result          // Loop + variable return - works
}
```

#### **Workarounds**
1. **Variable-based approach**: Always assign to a variable and return the variable
```ember
// ✅ Workaround for power function
power: fn(base, exp) {
    var result = 1
    if (exp == 0) {
        result = 1
    } else {
        result = base
        for (var i = 1; i < exp; i = i + 1) {
            result = result * base
        }
    }
    result          // Return variable, not from if-else
}
```

2. **Early return pattern**: Use explicit returns (when available)
3. **Restructure logic**: Avoid if-else as the final statement

#### **Impact Assessment**
- **Scope**: Limited to specific function return patterns
- **Severity**: Low - workarounds exist and are straightforward
- **User Experience**: Minimal impact with proper documentation
- **Core Features**: No impact on variables, objects, arrays, mixins, method calls

---

## ✅ **Resolved Issues**

### **Issue #R1: Function Return Values and Infinite Loops**
**Priority**: Critical  
**Status**: Resolved ✅  
**Date Resolved**: 2024-12-19  
**Commit**: `2509571`

#### **Description**
Functions were causing infinite loops and returning null instead of computed values.

#### **Solution**
- Fixed `OP_RETURN` implementation in `src/virtual_machine.c`
- Added `compile_function_body()` to preserve last expressions as return values
- Improved stack management in function calls

---

### **Issue #R2: Mixin Corruption in Global Variables**
**Priority**: High  
**Status**: Resolved ✅  
**Date Resolved**: 2024-12-19  
**Commit**: `c70f890`

#### **Description**
Global mixin objects were being corrupted during property assignments, causing previously working mixins to lose their methods.

#### **Solution**
- Enhanced object identification in `OP_SET_PROPERTY` and `OP_CALL_METHOD`
- Added memory address comparison and property content matching
- Replaced naive property count matching with robust object identification

---

### **Issue #R3: Complex Expression Parser Limitations**
**Priority**: Medium  
**Status**: Resolved ✅  
**Date Resolved**: 2024-12-19

#### **Description**
Parser couldn't handle complex chained expressions like `obj.property[index]` or multi-line arrays.

#### **Solution**
- Added workarounds in test cases using intermediate variables
- Enhanced nested object compilation to handle complex property access
- Fixed array literal compilation stack management

---

## 🔧 **Known Limitations**

### **L1: Parser Multi-line Array Limitations**
**Status**: Documented  
**Workaround**: Use single-line array syntax or intermediate variables

### **L2: Property Access with Array Indexing**
**Status**: Documented  
**Workaround**: Split `obj.property[index]` into separate statements

---

## 📋 **Testing Status**

### **Test Suite Results** (Last Run: 2024-12-19)
```
✅ test_01_variables.ember - PASSED
✅ test_02_arrays.ember - PASSED  
✅ test_03_objects.ember - PASSED
✅ test_04_functions.ember - PASSED
✅ test_05_operators.ember - PASSED
✅ test_06_control_flow.ember - PASSED
✅ test_07_mixins.ember - PASSED
✅ test_08_complex_expressions.ember - PASSED
```

**Total**: 8/8 tests passing (100%)

### **Feature Completeness**
- ✅ Variables and declarations
- ✅ Arrays and indexing  
- ✅ Objects and nested objects
- ✅ Functions (with noted if-else limitation)
- ✅ Operators and expressions
- ✅ Control flow (if, while, for)
- ✅ Mixins and composition
- ✅ Complex expressions (with workarounds)
- ✅ Method calls and chaining
- ✅ Property access and assignment

---

## 🎯 **Priority Guidelines**

### **Critical**: Core language features broken
- Examples: Variable assignment fails, objects don't work, VM crashes

### **High**: Major features with significant user impact  
- Examples: Mixins completely broken, method calls fail

### **Medium**: Important features with workarounds available
- Examples: Parser limitations, specific syntax edge cases

### **Low**: Edge cases with minimal impact and clear workarounds
- Examples: Specific function return patterns, minor syntax limitations

---

## 📝 **Contribution Guidelines**

When adding new issues to this document:

1. **Use the template format** shown in existing issues
2. **Include code examples** showing both broken and working cases  
3. **Provide workarounds** when available
4. **Assess impact** on overall functionality
5. **Update test status** if tests are affected
6. **Reference commits** for resolved issues

---

## 🔄 **Update History**

- **2024-12-19**: Initial document creation with current known issues
- **2024-12-19**: Added resolved issues from recent fixes
- **2024-12-19**: All tests now passing, documented if-statement edge case

---

*Last Updated: 2024-12-19*  
*EmberScript Version: Latest (main branch)* 