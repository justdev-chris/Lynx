
# 🐾 Lynx v1.3

Lynx is a lightweight, animal-themed scripting engine built in C. It supports modular packs, persistent memory stashing, and basic arithmetic for rapid tool development.

## 🚀 Quick Start

1. **Compile the engine:**
   ```
   gcc main.c scanner.c parser.c memory.c -o lynx.exe
2. **Run a script:**
```
lynx myscript.lnx
```
3. **Enter Interactive Mode (REPL):**
```
lynx

```



---

## 📖 Syntax & Commands

### Variable Management

* `Set [name] = [value]` : Store a numeric value in the Den.
* `Hunt` : Displays all variables currently stored in the Den.
* `Roar [name]` : Prints the value of a variable to the console.

### Arithmetic

Lynx supports basic math during the `Set` command:

* `Set health = 100 - 20`
* `Set area = radius * radius`
* `Set total = a + b`

### Modules & Persistence

* `Stalk_Pack "filename.lnx"` : Imports another Lynx file. It searches the root and the `std/` folder.
* `Stash "filename.lnx"` : Exports the current Den contents into a reusable Lynx script.

### Terminal Colors

If you import `colors.lnx`, you can change the terminal output color by Roaring the color name:

```
Stalk_Pack "colors.lnx"
Roar green
Roar score
Roar reset

```

---

## 🛠️ Project Structure

* `main.c` : Entry point and file loading logic.
* `parser.c` : The "Brain" that interprets commands and math.
* `scanner.c` : Breaks text into tokens for the parser.
* `memory.c` : The "Den" where all variables are stored.
* `std/` : Standard library containing `math.lnx` and `colors.lnx`.

---

## 🤖 Automation

This project includes a GitHub Action workflow (`.github/workflows/build.yml`) that automatically:

1. Compiles the engine on Windows.
2. Runs integration tests for math, stashing, and packs.
3. Uploads the `lynx.exe` binary as a build artifact.

