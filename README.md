
# Ember

<p align="center">
  <img src="./EmberScript-Logo.png" alt="EmberScript Logo" width="200" />
</p>

A lightweight, portable programming language for both embedded use and stand-alone executables. Written entirely in C, Ember emphasizes:

### Performance & Minimal Overhead
A compact stack-based VM and optional AST interpreter are included, enabling usage from quick scripting tasks to entire application development.

### Simplicity & Extensibility
A clean syntax drawing inspiration from Lua, AngelScript, and Haxe.

### Adaptability
Capable of scaling from minimal environments to large-scale applications.

---

## Key Highlights

### Pure C Implementation
Ensures broad platform support and straightforward integration.

### Bytecode Compilation
Supports compilation to a fast, stack-based VM using the `emberc` command-line compiler, or interpretation of the AST directly.

### Feature-Rich
Encompasses coroutines, optional static typing, a user-friendly syntax, and other advanced capabilities.

### Standard Library & Modules
Provides a core library, with the ability to import and extend functionality through custom modules.

### Package Management
Offers `emberpm` to install and manage Ember modules.

### Future Roadmap
Macros and compile-time meta-programming, pattern matching, hot reloading, inline bytecode, advanced memory operations, and more.

---

Ember aims to blend the convenience of dynamic scripting languages with the reliability of statically typed systems — all within a minimalist, modern design. Contributions and feedback are always welcome!

---

## Example: Future Features in Action

Below is a **demonstration script** showcasing many of Embers’s planned and partially implemented features — from modules/namespaces, typed variables, and traits, to coroutines, async/await, FRP streams, property observers, operator overloading, and more. While not all of these are fully functional in the current build, this script illustrates the **long-term roadmap** features:

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
let string greeting = "Welcome to Ember Next-Gen!";

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

hotReload();

// ---------------------------------------------------------
// (M) Inline Opcode Example with memory + bitwise ops
// ---------------------------------------------------------
print("Using inline opcode to manipulate memory and do bitwise operations");

// Suppose we want to fill a buffer of 1000 bytes with the value 42, 
// then do some bitwise transformations on certain entries.
inline_opcode {
    // 1) Allocate 1000 bytes
    LOAD_CONST 1000
    MEM_ALLOC
    // top of stack is 'ptr'
    STORE_VAR 0   // store pointer in global var 0

    // 2) Initialize i=0
    LOAD_CONST 0
    STORE_VAR 1   // store i in var 1

LOOP_START:
    // if i >= 1000 => exit
    LOAD_VAR 1
    LOAD_CONST 1000
    OP_GTE             // i >= 1000?
    JUMP_IF_TRUE 12,0  // jump to LOOP_END if true; offset is hypothetical

    // store 42 at (ptr + i)
    LOAD_VAR 0  // ptr
    LOAD_VAR 1  // i
    LOAD_CONST 42
    MEM_STORE8

    // i++ => i = i + 1
    LOAD_VAR 1
    LOAD_CONST 1
    ADD
    STORE_VAR 1

    // jump back to LOOP_START
    LOOP 30,0 // negative offset

LOOP_END:
    // Let's pick an offset in the buffer to do a bitwise transform, 
    // e.g. offset = 500
    LOAD_VAR 0
    LOAD_CONST 500
    MEM_LOAD8
    // => top of stack is the byte we read (expected 42).

    // SHIFT LEFT by 2 bits
    LOAD_CONST 2
    SHIFT_LEFT  
    // => top of stack is 42 << 2 = 168

    // Now AND it with 0xFF (just to demonstrate bitmasking)
    LOAD_CONST 255
    BIT_AND
    // => top of stack is 168 & 0xFF = 168

    // Store it back into memory at (ptr + 500)
    LOAD_VAR 0
    LOAD_CONST 500
    // top of stack is still 168
    MEM_STORE8

    // Now read that back and print
    LOAD_VAR 0
    LOAD_CONST 500
    MEM_LOAD8
    PRINT

    // Finally, free the memory
    LOAD_VAR 0
    MEM_FREE
}

print("Inline memory + bitwise ops done!");

print("=== The next-generation Ember adventure has ended. ===");
```
<br/>

## License

Ember is licensed under the MIT License. See [LICENSE](LICENSE) for details.
