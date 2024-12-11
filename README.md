
# EmberScript

<p align="center">
  <img src="./EmberScript-Logo.png" alt="EmberScript Logo" width="200" />
</p>

EmberScript is a lightweight, extensible, and highly portable scripting language designed for embedding in games and applications. Written entirely in pure C, EmberScript offers high performance and minimal overhead, making it ideal for resource-constrained environments. Combining simplicity and flexibility, EmberScript is perfect for defining game logic, creating in-game events, and scripting interactive behaviors.

The long-term vision for EmberScript is to evolve into a powerful, game-focused scripting language that blends the best features from other popular languages like Lua, AngelScript, and Haxe. By incorporating features such as lightweight coroutines, strong typing and flexible table structures, EmberScript aims to deliver the perfect balance between usability and power.

## Features
- **Dynamic Typing:** EmberScript provides the flexibility of dynamic typing, making it ideal for rapid prototyping and game logic.
- **Built-in Functions:** Extend the language with custom built-in functions to interact with your engine or application.
- **Interoperability:** EmberScript is written in pure C. Easily integrate EmberScript into your C or C++ projects with the lightweight interpreter.
- **Extensible Syntax:** A simple, clear syntax inspired by modern scripting languages.
- **Future Roadmap:** Planned features include coroutines, modules, type inference, and pattern matching.
- **Core Language Features**:
  - Variables and constants
  - Functions
  - Conditionals (`if`, `else`)
  - Loops (`while`, `for`)
  - Basic arithmetic and string manipulation
  - Built-in print function for debugging

## Quick Start

### Embedding EmberScript

1. Clone the repository:
   ```bash
   git clone https://github.com/your-username/EmberScript.git
   cd EmberScript
   ```

2. Compile the project:
   ```bash
   make
   ```

3. Include EmberScript in your project:
TODO

4. Use the EmberScript interpreter in your application:
TODO

### Writing Scripts

Create a script file (`example.ember`):
```javascript
var gold = 0;
var health = 100;

function battle(enemy, damage) {
    print("A wild " + enemy + " appears!");
    while (health > 0 && damage > 0) {
        print("You attack the " + enemy + "!");
        damage = damage - 25;
        if (damage <= 0) {
            print("You defeated the " + enemy + "!");
            gold = gold + 20;
        } else {
            print("The " + enemy + " attacks you!");
            health = health - 30;
        }
    }
}

battle("Goblin", 100);
```

Run your application to see the script in action!

## Contributing

We welcome contributions to EmberScript! If you'd like to contribute:
1. Fork the repository.
2. Create a feature branch: `git checkout -b feature-name`.
3. Commit your changes: `git commit -m 'Add new feature'`.
4. Push to the branch: `git push origin feature-name`.
5. Create a pull request.

## Roadmap

- **Core Features**:
  - [x] Variable support
  - [x] Function declarations
  - [x] Basic conditionals and loops
  - [ ] Array support
  - [ ] Debugger support

- **Lua-Inspired Features**:
  - [ ] Flexible table structures (combined arrays and dictionaries)
  - [ ] Coroutines for asynchronous execution
  - [ ] Sandboxing for secure script execution

- **AngelScript-Inspired Features**:
  - [ ] Strong typing with generics
  - [ ] Custom operators and operator overloading
  - [ ] Event-based architecture

- **Haxe-Inspired Features**:
  - [ ] Macros and compile-time meta-programming
  - [ ] Pattern matching for cleaner conditional logic
  - [ ] Null safety to reduce runtime errors
  - [ ] Type inference for simpler syntax
  - [ ] Abstract types to enhance primitive behaviors

- **General Features**:
  - [ ] Modules and namespaces for better project structure
  - [ ] Dynamic methods for runtime flexibility
  - [ ] Serialization/deserialization for save/load functionality
  - [ ] Conditional compilation for platform-specific behavior

---

## License

EmberScript is licensed under the GPL-3.0 License. See [LICENSE](LICENSE) for details.
