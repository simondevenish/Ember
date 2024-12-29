
# EmberScript

<p align="center">
  <img src="./EmberScript-Logo.png" alt="EmberScript Logo" width="200" />
</p>

EmberScript is a lightweight, extensible, and highly portable scripting language designed for embedding in games and applications. Written entirely in pure C, EmberScript provides high performance and minimal overhead—perfect for resource-constrained environments. Combining simplicity and flexibility, EmberScript excels at defining game logic, creating in-game events, and scripting interactive behaviors.

EmberScript can be run directly by parsing its Abstract Syntax Tree (AST) and interpreting it on the fly, or it can be compiled to bytecode and executed on a lightweight, stack-based Virtual Machine—giving developers the freedom to choose whichever approach best suits their project.

Long term, EmberScript aspires to merge the best features from Lua, AngelScript, and Haxe—such as lightweight coroutines, strong typing, and flexible table structures—all while maintaining an accessible, modern syntax. By striking a balance between usability and power, EmberScript aims to become the ideal choice for scripting and rapid prototyping in both small and large-scale game projects.

## Features
- **Two Execution Modes:** Choose between **direct AST interpretation** for rapid development or **bytecode compilation** for running scripts on a lightweight, stack-based **VM**.
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
// Demonstrates variables, arrays, functions, conditionals, loops, arithmetic, and printing

var gold = 0;
var health = 100;
var items = ["Sword", "Potion", "Shield"];

print("Starting adventure with health = " + health);

// A simple function to simulate a battle
function battle(enemy, damage) {
    print("A wild " + enemy + " appears!");
    
    while (health > 0 && damage > 0) {
        print("You attack the " + enemy + " with your " + items[0] + "!");
        damage = damage - 25;
        
        if (damage <= 0) {
            gold = gold + 20;
            print("You defeated the " + enemy + "! Loot gained: 20 gold.");
        } else {
            print("The " + enemy + " fights back!");
            health = health - 30;
            
            if (health <= 0) {
                print("You have been defeated...");
            }
        }
    }
}

// Show all items in the inventory with a fixed-size loop
for (var i = 0; i < 3; i = i + 1) {
    print("Item " + i + ": " + items[i]);
}

// Initiate a simple battle scenario
battle("Goblin", 50);

print("Final stats: health = " + health + ", gold = " + gold);

```

Run your application to see the script in action!

## Roadmap

- **Core Features**:
  - [x] Variable support
  - [x] Function declarations
  - [x] Basic conditionals and loops
  - [x] Array support
  - [ ] Debugger support

- **Lua-Inspired Features**:
  - [ ] Flexible table structures (combined arrays and dictionaries)
  - [ ] Coroutines for asynchronous execution (e.g. `yield`, as in `gatherResources()`)
  - [ ] Sandboxing for secure script execution

- **AngelScript-Inspired Features**:
  - [ ] Strong typing with generics (e.g. `function <T> combine(a: T, b: T): T`)
  - [ ] Custom operators and operator overloading (`operator +(...) { ... }`)
  - [ ] Event-based architecture (e.g. `on("playerDeath", fn)`)

- **Haxe-Inspired Features**:
  - [ ] Macros and compile-time meta-programming
  - [ ] Pattern matching for cleaner conditional logic (`match (h) { ... }`)
  - [ ] Null safety to reduce runtime errors (`maybeItem?.use()`)
  - [ ] Type inference for simpler syntax (`let number difficulty = 2;`)
  - [ ] Abstract types to enhance primitive behaviors (`abstract type Mana = number from { ... }`)

- **General Features**:
  - [ ] Modules and namespaces for better project structure (`import "utility.ember"; namespace epicAdventure { ... }`)
  - [ ] Dynamic methods for runtime flexibility
  - [ ] Serialization/deserialization for save/load functionality (`saveGame()`, `loadGame()`)
  - [ ] Conditional compilation for platform-specific behavior (`#if DEBUG ... #else ... #end`)
  - [ ] Property observers (`willSet`/`didSet`)  
  - [ ] FRP streams (`healthStream.onChange(fn) { ... }`)
  - [ ] Permissions (security & restricted APIs) (`permissions({ network: false }, fn) { ... }`)
  - [ ] Chainable table updates (`player.set("health", 70).update("stats", fn) { ... }`)
  - [ ] Async/await (futures/promises) (`async function fetchData(url) { ... }`)
  - [ ] First-class FFI (foreign function interface) (`extern function C_Multiply(...)`)
  - [ ] Hot reloading (`hotReload()`)
  - [ ] Decorators (e.g., `@logCall`)
---

## Example: Future Features in Action

Below is a **demonstration script** showcasing many of EmberScript’s planned and partially implemented features — from modules/namespaces, typed variables, and traits, to coroutines, async/await, FRP streams, property observers, operator overloading, and more. While not all of these are fully functional in the current build, this script illustrates the **long-term roadmap** features:

```javascript
#if DEBUG
    print("=== Starting Game in DEBUG Mode ===");
#else
    print("=== Starting Game in RELEASE Mode ===");
#end

// ---------------------------------------------------------
// 1. Modules & Namespaces
// ---------------------------------------------------------
import "utility.ember";     // Hypothetical module with helper functions
import "network.ember";     // Hypothetical module for networking

namespace epicAdventure {
    // Some namespace-level constants
    const MAX_HEALTH = 100;
    const START_GOLD = 10;
}

// ---------------------------------------------------------
// 2. Declarations (Dynamic & Typed), Abstract Types, Traits
// ---------------------------------------------------------

var isGameRunning = true;  // A dynamic variable

// Typed variables (AngelScript/Haxe style)
let number difficulty = 2;  
let string greeting = "Welcome to EmberScript Next-Gen!";

// Abstract type (Haxe-inspired) example
abstract type Mana = number from {
    function clamp(value: number): number {
        if (value < 0) return 0;
        if (value > 100) return 100;
        return value;
    }
};
let Mana magicPoints = 20;

// Demonstrate a trait (mixins for behavior composition)
trait Swimmable {
    function swim() {
        print(this.name + " is swimming gracefully!");
    }
}

trait MagicalBeing {
    function castSpell(spell) {
        print(this.name + " casts " + spell + "!");
    }
}

// ---------------------------------------------------------
// 2a. Tables with Property Observers (willSet / didSet)
// ---------------------------------------------------------
var player = {
    // When property observers are supported, you can define them like so:
    name = "Adventurer", 
    health = 50 {
        willSet(newVal) {
            print("(Observer) Health about to change from " + player.health + " to " + newVal);
        }
        didSet(oldVal) {
            if (player.health <= 0) {
                trigger("playerDeath");
            }
        }
    },
    gold = epicAdventure::START_GOLD,

    inventory = ["Sword", "Potion"],
    stats = { strength = 12, agility = 8, magic = 4 },

    methods = {
        attack = function(target) {
            print(player.name + " attacks " + target + " with " + player.inventory[0]);
        }
    }
};

// Mixin traits into our player
mixin(player, Swimmable, MagicalBeing);

// ---------------------------------------------------------
// 3. Built-in Print, Debugger & Setup
// ---------------------------------------------------------
print(greeting);
print("Your name is: " + player.name);
print("Initial gold: " + player.gold);
debugBreak(); // Hypothetical debugger breakpoint

// ---------------------------------------------------------
// 4. Event System
// ---------------------------------------------------------
on("playerDeath", function() {
    print("[EVENT] Player has died. Game Over!");
    isGameRunning = false;
});

// ---------------------------------------------------------
// 5. Functions, Conditionals, Loops, Operator Overloading
// ---------------------------------------------------------

// (A) Basic function
function displayStats(p) {
    print("--- Player Stats ---");
    print("Name: " + p.name);
    print("Health: " + p.health);
    print("Gold: " + p.gold);
    print("Inventory: " + p.inventory);
    print("Strength: " + p.stats.strength);
    print("Agility: " + p.stats.agility);
    print("Magic: " + p.stats.magic);
    print("--------------------");
}

// (B) Default Parameter Example
function greetCharacter(name = "Traveler") {
    print("Hello, " + name + "!");
}

// (C) Spread/Rest Parameters
function logAll(...messages) {
    foreach msg in messages {
        print("LOG: " + msg);
    }
}

// (D) Healing function (with return)
function heal(amount) {
    player.health = player.health + amount;
    if (player.health > epicAdventure::MAX_HEALTH) {
        player.health = epicAdventure::MAX_HEALTH;
    }
    print("Healed by " + amount + "! Health is now: " + player.health);
    return player.health;
}

// (E) Generics (AngelScript-inspired)
function <T> combine(a: T, b: T): T {
    return a + b; 
}

// (F) Enhanced Pattern Matching
function checkHealth(h) {
    // Suppose we match on multiple ranges or patterns
    match (h) {
        0:            print("You are dead!");
        ..20:         print("You are severely injured!");
        21..50:       print("You are wounded!");
        100:          print("You are at full health!");
        default:      print("Current health is: " + h);
    }
}

// (G) Foreach loop example
var loot = ["Apple", "Gemstone", "Shield"];
foreach item in loot {
    print("You found loot: " + item);
}

// (H) Comprehensions (Pythonic)
let squares = [ x*x for x in [1,2,3,4] ];
print("Squares: " + squares);  // => [1,4,9,16]

// (I) Null safety example
function handleNullSafety() {
    var maybeItem = null;
    maybeItem?.use(); 
    print("Null safety test completed.");
}

// (J) Operator Overload
operator +(var left, var right) {
    if (typeOf(left) == "object" && typeOf(right) == "number") {
        left.gold += right;
        return left;
    }
    return left + right;
}

// (K) Chainable Table Updates
function powerUpPlayer() {
    player
      .set("health", player.health + 20)
      .update("stats", fn(oldStats) { 
          oldStats.strength += 5; 
          return oldStats;
      })
      .set("name", "Super " + player.name);
}

// (L) Decorator Syntax (hypothetical)
@logCall
function craftItem(itemName) {
    print("Crafting a " + itemName + "...");
}

// ---------------------------------------------------------
// 6. Coroutines (Lua-inspired)
// ---------------------------------------------------------
function gatherResources() {
    print("Starting resource gathering...");
    yield;  
    print("Gathered some rare materials!");
    yield;
    print("Resource gathering complete!");
}

// ---------------------------------------------------------
// 7. Async/Await (Futures/Promises Model)
// ---------------------------------------------------------
async function fetchData(url) {
    // Suppose network.fetch returns a promise-like object
    let data = await network.fetch(url);
    print("Fetched data from " + url + ": " + data);
    return data;
}

// ---------------------------------------------------------
// 8. FRP Streams Example
// ---------------------------------------------------------
var healthStream = createStream(player.health);
healthStream.onChange(fn(newVal) {
    print("[FRP] Health changed to " + newVal);
});

// ---------------------------------------------------------
// 9. Security & Permissions
// ---------------------------------------------------------
permissions({ 
    network: false, 
    fileIO: false 
}, function() {
    print("Inside a restricted zone — no network or file IO allowed!");
});

// ---------------------------------------------------------
// 10. First-Class FFI Example
// ---------------------------------------------------------
extern function C_Multiply(a: number, b: number): number;
function testFFI() {
    let result = C_Multiply(6, 7);
    print("FFI call C_Multiply(6, 7) -> " + result);
}

// ---------------------------------------------------------
// 11. Hot Reloading
// ---------------------------------------------------------
// Hypothetical function that re-parses the script at runtime
function hotReload() {
    print("Hot Reloading game scripts...");
    reloadScript("exhibit.ember"); // recompile & re-bind?
    print("Scripts reloaded successfully!");
}

// ---------------------------------------------------------
// 12. Serialization/Deserialization for Save/Load
// ---------------------------------------------------------
function saveGame() {
    var saveData = serialize(player);
    print("Game saved! Data: " + saveData);
    return saveData;
}

function loadGame(saveData) {
    var loadedPlayer = deserialize(saveData);
    print("Game loaded! Player name: " + loadedPlayer.name);
    return loadedPlayer;
}

// ---------------------------------------------------------
// 13. Main Game Flow
// ---------------------------------------------------------
displayStats(player);

// Kick off a coroutine
var co = coroutine.create(gatherResources);
print("[MAIN] Resuming gatherResources #1");
coroutine.resume(co); 
print("[MAIN] Doing other tasks...");
print("[MAIN] Resuming gatherResources #2");
coroutine.resume(co);
print("[MAIN] Resuming gatherResources #3");
coroutine.resume(co);

// Demonstrate destructuring & multiple assignment
var [obj1, obj2, obj3] = loot;
print("Destructured loot items: " + obj1 + ", " + obj2 + ", " + obj3);

var { name, gold } = player;
print("Destructured player name = " + name + ", gold = " + gold);

// Basic battle
battle("Goblin", 40);
displayStats(player);

if (!isGameRunning) {
    print("Game ended prematurely...");
} else {
    print("You survived the goblin. Continuing...");
}

// Heal up, check health
heal(30);
checkHealth(player.health);

// Demonstrate generics
let number comboNum = combine<number>(10, 15);
let string comboStr = combine<string>("Hello, ", player.name);
print("combine<number>(10, 15) -> " + comboNum);
print("combine<string>(\"Hello, \", player.name) -> " + comboStr);

// Another loop
for (var i = 0; i < 3; i = i + 1) {
    print("Exploration step " + i);
}

// Add 50 gold (operator overload)
print("Adding 50 gold using '+' operator...");
player + 50; 
print("New gold total: " + player.gold);

// Attack using dynamic method + trait
player.methods.attack("Dragon Egg");
player.swim();       // from Swimmable
player.castSpell("Fireball"); // from MagicalBeing

// Final battle
battle("Dragon", 100);
if (!isGameRunning) {
    print("Your journey ends in darkness...");
} else {
    print("You have slain the Dragon!");
    player.gold += 100;
    print("Final treasure count: " + player.gold);
}

// Demonstrate chainable table updates
powerUpPlayer();
displayStats(player);

// Save & Load demonstration
var savedState = saveGame();
print("Modifying player name to test load...");
player.name = "Mysterious Stranger";
print("Loading game from saved data...");
player = loadGame(savedState);
displayStats(player);

// Test FFI
testFFI();

// Possibly do a hot reload
hotReload();

print("=== The next-generation EmberScript adventure has ended. ===");
```
<br/>

## License

EmberScript is licensed under the GPL-3.0 License. See [LICENSE](LICENSE) for details.
