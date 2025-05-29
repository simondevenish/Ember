# EmberScript Event System Design

## Table of Contents
- [Overview](#overview)
- [Core Syntax](#core-syntax)
- [Filter System](#filter-system)
- [Game Development Scenarios](#game-development-scenarios)
- [Implementation Strategy](#implementation-strategy)
- [Technical Specifications](#technical-specifications)
- [Performance Considerations](#performance-considerations)
- [Future Extensions](#future-extensions)

## Overview

The EmberScript Event System provides a declarative, game-focused approach to event handling that eliminates boilerplate code and makes event-driven programming intuitive. The system is designed specifically for game development scenarios where objects need to react to player actions, AI behaviors, environmental changes, and UI updates.

### Design Goals
- **Declarative Event Binding**: Clear "this responds to that" relationships
- **Precise Targeting**: Filter system prevents event spam and improves performance
- **Inline Conditions**: Prevent unnecessary handler execution with built-in conditionals
- **Game-Focused**: Optimized for common game development patterns
- **Minimal Boilerplate**: Reduce the code needed for complex event interactions

## Core Syntax

### Event Binding (Listeners)

**Basic Syntax:**
```ember
functionName: fn() <- ["EventName" {condition} | filter1, filter2, ...]
  // function body
```

**Components:**
- `fn() <-`: Function declaration with event binding operator
- `["EventName"]`: Event name in square brackets
- `{condition}`: Optional inline condition (only execute if true)
- `| filter1, filter2`: Optional filter chain with pipe separator

**Examples:**
```ember
// Simple event binding
onPlayerJump: fn() <- ["PlayerJump"]
  print("Player jumped!")

// With condition
onLowHealth: fn() <- ["HealthChanged" {if this.health < 20}]
  print("Warning: Low health!")

// With filters
onEnemyAttack: fn() <- ["Attack" | type(enemy), target(player)]
  this.takeDamage(event.damage)

// Multiple filters
onNightEvent: fn() <- ["NightAlarm" {if isNightTime} | all, priority(high)]
  print("Night has fallen!")
```

### Event Broadcasting (Invocation)

**Basic Syntax:**
```ember
fire["EventName" {condition} | filter1, filter2]
```

**With Optional Body:**
```ember
fire["EventName" {condition} | filter1, filter2]
  // inline actions during broadcast
```

**Examples:**
```ember
// Simple broadcast
fire["PlayerJump"]

// With condition
fire["WeatherAlert" {if temperature < 0}]

// With filters and inline action
fire["BossDefeated" | all, priority(critical)]
  print("Victory! The boss has been defeated!")

// Complex broadcast with multiple conditions
fire["CombatStart" {if player.isAlive && enemy.isAlive} | participants(player, enemy)]
  print("Combat begins between " + player.name + " and " + enemy.name)
```

## Filter System

### Built-in Filters

| Filter | Description | Example |
|--------|-------------|---------|
| `all` | All registered listeners | `\| all` |
| `type(T)` | Objects of specific type | `\| type(player)` |
| `role(R)` | Objects with specific role | `\| role(guard)` |
| `name(N)` | Objects with specific name | `\| name(Hero)` |
| `location(L)` | Objects in specific location | `\| location(town)` |
| `near(obj)` | Objects near specified object | `\| near(player)` |
| `priority(P)` | Priority level (high/medium/low) | `\| priority(high)` |
| `ui` | UI-related listeners only | `\| ui` |
| `debug(bool)` | Debug-conditional listeners | `\| debug(true)` |
| `target(obj)` | Specific target object | `\| target(enemy)` |
| `owner(obj)` | Objects owned by specified object | `\| owner(player)` |

### Custom Property Filters

```ember
// Numeric comparisons
| health(>50)        // Objects with health > 50
| level(>=10)        // Objects with level >= 10
| distance(<100)     // Objects within 100 units

// String matching
| faction(allied)    // Objects with faction="allied"
| status(active)     // Objects with status="active"

// Boolean properties
| isAlive(true)      // Objects where isAlive=true
| canFly(false)      // Objects where canFly=false
```

### Filter Combinations

```ember
// Multiple filters are AND-ed together
onGuardAlert: fn() <- ["Alarm" | type(npc), role(guard), location(city)]

// Different broadcasts can target different groups
alertSystem: fn()
  fire["Emergency" | type(player), priority(critical)]
  fire["Emergency" | type(npc), priority(medium)]
  fire["Emergency" | ui, priority(high)]
```

## Game Development Scenarios

### 1. Combat System

```ember
var player = {
  name: "Hero"
  health: 100
  mana: 50
  isAlive: true

  // Take damage from attacks
  takeDamage: fn() <- ["AttackHit" {if this.isAlive} | target(this)]
    this.health -= event.damage
    print(this.name + " takes " + event.damage + " damage!")
    
    fire["HealthChanged" | target(this), ui]
    
    if this.health <= 0
      this.isAlive = false
      fire["PlayerDied" | all, priority(critical)]

  // Cast spells
  castFireball: fn() <- ["SpellCast" {if this.mana >= 20} | caster(this), spell(fireball)]
    this.mana -= 20
    fire["ProjectileLaunched" | target(nearestEnemy), damage(25), type(fire)]

  // Level up celebration
  onLevelUp: fn() <- ["ExperienceGained" {if this.exp >= this.nextLevel} | target(this)]
    this.level += 1
    this.nextLevel *= 1.5
    fire["LevelUp" | target(this), newLevel(this.level), ui]
}

var enemy = {
  name: "Goblin"
  health: 30
  ai: "aggressive"

  // AI reaction to player actions
  reactToSpell: fn() <- ["ProjectileLaunched" {if event.target == this} | target(this)]
    if event.type == "fire" && this.weakness == "fire"
      this.takeDamage(event.damage * 2)
    el
      this.takeDamage(event.damage)

  // Death behavior
  onDeath: fn() <- ["HealthChanged" {if this.health <= 0} | target(this)]
    fire["EnemyDefeated" | killer(event.attacker), enemy(this)]
    this.dropLoot()
}

// UI System
var healthBar = {
  updateDisplay: fn() <- ["HealthChanged" | ui, priority(high)]
    print("Health bar updated: " + event.target.health + "/" + event.target.maxHealth)
}

var levelUpEffect = {
  showCelebration: fn() <- ["LevelUp" | ui, priority(medium)]
    print("✨ LEVEL UP! You are now level " + event.newLevel + "! ✨")
    fire["ParticleEffect" | type(celebration), duration(3)]
}
```

### 2. Day/Night Cycle & Weather

```ember
var timeSystem = {
  hour: 8
  isNight: false
  weather: "clear"

  advanceTime: fn()
    this.hour += 1
    if this.hour >= 24
      this.hour = 0
      fire["NewDay" | all]
    
    if this.hour == 20 && !this.isNight
      this.isNight = true
      fire["NightFalls" | all, priority(medium)]
    
    if this.hour == 6 && this.isNight
      this.isNight = false
      fire["DawnBreaks" | all, priority(medium)]

  changeWeather: fn(newWeather)
    this.weather = newWeather
    fire["WeatherChanged" | all, weather(newWeather)]
}

var player = {
  name: "Adventurer"
  
  // React to night time
  onNight: fn() <- ["NightFalls" {if timeSystem.isNight}]
    print("It's getting dark... I should find shelter.")
    fire["PlayerAction" | action(seekShelter)]

  // React to storms
  onStorm: fn() <- ["WeatherChanged" {if event.weather == "storm"} | type(player)]
    print("A storm is brewing! Taking cover.")
    this.stamina -= 10
}

var townGuard = {
  name: "Night Watch"
  
  // Start night patrol
  beginNightWatch: fn() <- ["NightFalls" | role(guard), location(town)]
    print("Guard " + this.name + " begins night patrol")
    fire["TorchesLit" | location(town), ui]
}

var wildlife = {
  wolves: []
  
  // Wolves become active at night
  wolvesEmerge: fn() <- ["NightFalls" {if timeSystem.hour >= 21} | type(wildlife)]
    print("Wolves howl in the distance...")
    fire["DangerLevel" | level(increased), creatures(wolves)]
}
```

### 3. Inventory & Trading System

```ember
var inventory = {
  items: []
  maxSlots: 20
  gold: 100

  // Add items with capacity check
  addItem: fn() <- ["ItemPickup" {if this.items.length < this.maxSlots} | target(this)]
    this.items.push(event.item)
    print("Picked up: " + event.item.name)
    fire["InventoryChanged" | owner(this), ui]
    
    // Check for special items
    if event.item.rarity == "legendary"
      fire["LegendaryFound" | all, item(event.item)]

  // Use items
  useItem: fn() <- ["ItemUse" {if event.item in this.items} | owner(this)]
    var item = event.item
    print("Using " + item.name)
    
    // Apply item effects
    if item.type == "potion"
      fire["PotionEffect" | target(event.user), effect(item.effect)]
    el if item.type == "weapon"
      fire["WeaponEquipped" | target(event.user), weapon(item)]

  // Trading
  sellItem: fn() <- ["TradeAction" {if event.action == "sell"} | trader(this)]
    this.gold += event.price
    this.removeItem(event.item)
    fire["GoldChanged" | owner(this), amount(this.gold), ui]
}

var merchant = {
  name: "Shop Keeper"
  reputation: 0
  
  // React to legendary items
  reactToLegendary: fn() <- ["LegendaryFound" | type(merchant), near(player)]
    print("Merchant: 'My word! That's a legendary " + event.item.name + "!'")
    this.reputation += 1
    fire["MerchantReaction" | merchant(this), mood(impressed)]

  // Trading offers
  makeOffer: fn() <- ["TradeAction" {if event.action == "sell"} | near(this)]
    var price = event.item.value * (0.7 + this.reputation * 0.1)
    print("Merchant offers " + price + " gold for " + event.item.name)
    fire["TradeOffer" | price(price), item(event.item)]
}

// Global economy system
var economy = {
  updatePrices: fn() <- ["ItemPickup", "TradeAction" | all, system(economy)]
    // Adjust item values based on supply/demand
    if event.item.type == "rare_ore"
      this.adjustPrice(event.item, "increase")
}
```

### 4. AI Behavior & Social Systems

```ember
var npc = {
  name: "Villager"
  mood: "neutral"
  relationship: 0
  memory: []

  // React to player's good deeds
  witnessGoodDeed: fn() <- ["PlayerAction" {if event.karma > 0} | witness(this), type(npc)]
    this.relationship += event.karma
    this.mood = "happy"
    this.memory.push("good_deed_" + event.action)
    print(this.name + " smiles approvingly at your kindness")
    fire["RelationshipChanged" | npc(this), player(event.actor)]

  // React to violence nearby
  fearViolence: fn() <- ["Combat" {if event.distance < 30} | civilian, nearby]
    this.mood = "scared"
    print(this.name + " cowers in fear!")
    fire["NPCFled" | npc(this), location(this.currentLocation)]

  // Daily routine
  dailyGreeting: fn() <- ["DawnBreaks" | type(npc), routine(daily)]
    if this.relationship > 10
      print(this.name + " waves cheerfully: 'Good morning, friend!'")
      fire["FriendlyGreeting" | npc(this), target(player)]
    el
      print(this.name + " nods politely")
}

var guard = {
  name: "City Guard" 
  alertLevel: "normal"
  
  // Respond to crimes
  respondToCrime: fn() <- ["Combat" {if event.location == "city"} | role(guard), authority]
    this.alertLevel = "high"
    print("Guard shouts: 'Stop this violence at once!'")
    fire["LawEnforcement" | criminals(event.participants), authority(this)]

  // Investigate disturbances
  investigate: fn() <- ["NPCFled" {if event.location == this.patrol_area} | role(guard)]
    print("Guard " + this.name + " investigates the disturbance")
    fire["Investigation" | location(event.location), officer(this)]
}

// Global reputation system
var reputation = {
  playerKarma: 0
  
  updateKarma: fn() <- ["PlayerAction" | all, system(reputation)]
    if event.karma > 0
      this.playerKarma += event.karma
      fire["ReputationGained" | amount(event.karma)]
    el
      this.playerKarma += event.karma  // karma is negative
      fire["ReputationLost" | amount(Math.abs(event.karma))]

  // Reputation affects NPC interactions
  modifyInteractions: fn() <- ["NPCInteraction" | all, system(reputation)]
    if this.playerKarma > 50
      event.modifier = "friendly"
    el if this.playerKarma < -20
      event.modifier = "hostile"
    el
      event.modifier = "neutral"
}
```

## Implementation Strategy

### Phase 1: Core Event System (Week 1-2)
1. **Event Registry**: Global registry to store event listeners
2. **Basic Binding**: Parse and implement `fn() <- ["EventName"]` syntax
3. **Simple Broadcasting**: Implement `fire["EventName"]` functionality
4. **Event Objects**: Create event data structures with metadata

### Phase 2: Conditions & Basic Filters (Week 3-4)
1. **Inline Conditions**: Parse and evaluate `{if condition}` blocks
2. **Basic Filters**: Implement `all`, `type()`, `name()` filters
3. **Filter Chain Processing**: Handle multiple filters with `|` separator
4. **Event Context**: Pass event data to handlers (`event.target`, `event.damage`, etc.)

### Phase 3: Advanced Filters & Performance (Week 5-6)
1. **Custom Property Filters**: Implement `health(>50)`, `level(>=10)` syntax
2. **Spatial Filters**: Implement `near()`, `location()`, `distance()` filters
3. **Priority System**: Handle `priority()` filter for execution order
4. **Performance Optimization**: Efficient listener lookup and filtering

### Phase 4: Advanced Features (Week 7-8)
1. **Event Chaining**: Events triggering other events with cycle detection
2. **Async Events**: Queue events for delayed or scheduled execution
3. **Event History**: Debug system for event tracking and replay
4. **Hot Reload**: Update event handlers without restarting

## Technical Specifications

### AST Node Types
```c
typedef enum {
    AST_EVENT_BINDING,     // fn() <- ["EventName" {...} | ...]
    AST_EVENT_BROADCAST,   // fire["EventName" {...} | ...]
    AST_EVENT_CONDITION,   // {if condition}
    AST_EVENT_FILTER,      // | filter1, filter2
    AST_FILTER_EXPRESSION  // type(player), health(>50), etc.
} EventASTNodeType;
```

### Event System Data Structures
```c
typedef struct EventListener {
    char* event_name;
    ASTNode* condition;        // Optional condition block
    FilterChain* filters;      // Chain of filters
    ASTNode* function_body;    // Function to execute
    void* owner_object;        // Object that owns this listener
    int priority;              // Execution priority
    struct EventListener* next;
} EventListener;

typedef struct EventRegistry {
    EventListener** buckets;   // Hash table of listeners
    int bucket_count;
    int total_listeners;
} EventRegistry;

typedef struct EventData {
    char* event_name;
    RuntimeValue* parameters;  // Event parameters (target, damage, etc.)
    void* source_object;       // Object that fired the event
    int timestamp;
    int event_id;
} EventData;
```

### Bytecode Operations
```c
typedef enum {
    OP_EVENT_BIND,        // Register event listener
    OP_EVENT_FIRE,        // Broadcast event
    OP_EVENT_FILTER,      // Apply filter to event
    OP_EVENT_CONDITION,   // Evaluate event condition
    OP_EVENT_PARAM,       // Access event parameter
} EventOpCode;
```

### Filter System Implementation
```c
typedef struct Filter {
    FilterType type;           // FILTER_TYPE, FILTER_PROPERTY, etc.
    char* parameter;           // Filter parameter ("player", "health", etc.)
    ComparisonOp comparison;   // >, >=, ==, etc. (for property filters)
    RuntimeValue value;        // Comparison value
    struct Filter* next;       // Next filter in chain
} Filter;

typedef enum FilterType {
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
```

## Performance Considerations

### Event Listener Optimization
- **Hash Table**: Use event name as key for O(1) listener lookup
- **Filter Pre-computation**: Cache filter results for static conditions
- **Priority Queues**: Process high-priority events first
- **Lazy Evaluation**: Only evaluate conditions when filters pass

### Memory Management
- **Listener Pooling**: Reuse listener objects to reduce allocation
- **Event Data Cleanup**: Automatic cleanup of event parameters
- **Circular Reference Detection**: Prevent memory leaks in event chains
- **Weak References**: Use weak references for object-to-event relationships

### Scalability
- **Listener Limits**: Configurable limits on listeners per event
- **Event Queue Size**: Bounded queue for async events
- **Filter Complexity**: Limits on filter chain length
- **Condition Evaluation**: Timeout for complex condition expressions

## Future Extensions

### 1. Event Namespacing
```ember
// Hierarchical event names
onPlayerCombat: fn() <- ["player.combat.hit" | target(this)]

// Wildcard subscriptions
onAnyPlayerEvent: fn() <- ["player.*" | all]
```

### 2. Event Persistence
```ember
// Save/load event history
saveEventHistory("game_session.events")
replayEvents("last_combat.events")

// Event sourcing for game state
restoreGameState("checkpoint.events")
```

### 3. Remote Events (Multiplayer)
```ember
// Network event broadcasting
fire["PlayerMoved" | network, players(all)]

// Server-side event validation
onPlayerAction: fn() <- ["player.action" | validate(server)]
```

### 4. Visual Event Debugging
```ember
// Event flow visualization in IDE
@debug.trace
onComplexEvent: fn() <- ["GameEvent" | complex_filter]

// Real-time event monitoring
monitor.events(["Combat.*", "Player.*"])
```

### 5. Performance Profiling
```ember
// Event performance metrics
@profile.events
onExpensiveEvent: fn() <- ["HeavyComputation" | all]

// Automatic optimization suggestions
optimize.suggest_filters(["Combat.*"])
```

## Conclusion

The EmberScript Event System provides a powerful, declarative approach to event-driven programming that is specifically optimized for game development. The combination of inline conditions, sophisticated filtering, and clear syntax makes it easy to create complex behavioral systems while maintaining high performance and readability.

The phased implementation approach ensures that the core functionality can be delivered quickly, with advanced features added incrementally. The system's design prioritizes game development use cases while remaining flexible enough for general-purpose event handling.

---

*This document serves as the technical specification for implementing the EmberScript Event System. Implementation should follow the phased approach outlined above, with each phase thoroughly tested before proceeding to the next.* 