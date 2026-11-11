# Origin Script v0

A real, small interpreter executing inside Familiar Origin. It is not Python and does not run existing Python files. The language currently favors readable statements, exact integer semantics and bounded execution. It is not a performance claim or a neural parser.

## Try a single line

Enter `code print 2 + 3 * 4` to print 14. Enter `language` for a short syntax guide.

## Write a program

Create `demo.fo` in the native Files GUI, open Edit, and enter:

```
let count = 2
repeat 3
    set count to count * 2
end
say count
print "Hello from Origin Script"
```

Save with Ctrl+S, then select the file and click Run .fo, or enter `run demo.fo`. It prints 16 and the quoted text. Source comes from the actual RAM file. Closing/restarting the VM loses that file.

## Syntax and semantics

- `let NAME = expression` and `set NAME to expression` assign an integer variable. `l` abbreviates let. Assigning an existing name replaces its value within this one invocation.
- `print expression`, `say expression`, `show expression` and `p expression` print a value followed by a newline.
- Quoted strings support newline (`\n`), quote (`\"`) and backslash (`\\`) escapes. Strings are only supported as print arguments, not as variables.
- Expressions support signed integers, variables, parentheses, unary plus/minus, multiplication/division and addition/subtraction. Multiplication/division have higher precedence. Integer division truncates toward zero.
- Arithmetic overflow, division by zero, unknown variables and malformed input stop the program with a line-numbered error. Decimal positive literals are limited to 2,147,483,647; arithmetic results must fit signed 32-bit range.
- `repeat expression` starts a bounded loop, closed by `end` on its own line. The count is evaluated when the loop is entered, between 0 and 32. Nested loops are supported to depth four.
- Blank lines and lines beginning with `#` after leading whitespace are ignored. Names preserve case.
- `train hello` starts the real guest Transformer; `stop training` stops it; `generate hello` asks it to generate from current weights. Training is cooperative/asynchronous: these statements start/stop/request, they do not wait for convergence. A subsequent generate in the same script may still use untrained weights.

Each invocation has fresh variables. Limits: 511 source bytes, 64 lines, eight variables with names up to 15 characters, expression depth eight, loop depth four and at most 1,000 executed statements. There is no file/network/system API, third-party package loader, arbitrary machine-code execution, imports, functions, persistent state or general Python compatibility.

Known limit in 0.1.3: a program of exactly 64 lines must not end with a newline. The empty line after the last newline is counted as a 65th line and the program is refused.

## Human mistakes and exact actions

The three common print misspellings `prnit`, `pritn` and `pirnt` produce a specific suggestion and execute nothing for that statement. No ambiguous program is silently rewritten. Already completed earlier statements are not rolled back after a later error. The OS command field has its own broader bounded deterministic suggestion rules; neither parser is currently trained by a Transformer.

## Tests and next experiments

Host tests with ASan/UBSan cover precedence, assignment, loops, strings, arithmetic errors, limits and the guest-action callback. Real QEMU tests execute single-line statements and a `.fo` program read from the guest RAM filesystem on both architectures. No claim of fastest execution is made.

Possible next work: an explicit parsed representation, user-visible parse preview, richer diagnostics, structured agent form, more general string/file operations with a permission model, and a trained suggestion model evaluated against the deterministic parser. None is implied by v0.
