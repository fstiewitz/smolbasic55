## smolbasic55

smolbasic55 is at least a ECMA-55 Minimal BASIC compiler.

### Features

- Assembles to RISC-V or AMD64.
- Full ECMA-55 Minimal BASIC.
- Non-standard features are available.

### Usage

```
smolbasic55 [FEATURES] FILE.BAS OUTPUT.S
```

The generated assembly may need to be linked against the C files `data.c`, `array.c`, `input.c`, `print.c`, `control.c`, `string.c`, `math.c`, depending
on your code.

`FEATURES` are enabled with `+FEATURE` and disabled 
with `-FEATURE`. If `FEATURE` does not start with `NO`, `NOFEATURE` inverts the feature.

- `NOEND` allow missing `END`.
- `FULLDEF` make `DEF` more powerful (see below).
- `EXTERN` every function call that is neither user-defined nor builtin is treated as an external call (see below).
- `TYPE` add type suffixes (see below).
- `PTR` add untyped pointer casts (see below).
- `INLINE` any line that does not start with a (line) number is pasted verbatim into the assembly output.

Feature flags can also be set in a file with `10 OPTION FLAGS ...` (should be the first line number).

#### Option `FULLDEF`

If `FULLDEF` is set, user-defined functions can:

1. Have multiple arguments.
2. Have string arguments.
3. Return strings (e.g. `DEF FNA$(X$)=X$`).

#### Option `EXTERN`

If `EXTERN` is set:

1. `SOMEFUN(...)` in an expression translates to an external call that returns a floating-point number.
2. `SOMEFUN$(...)` in an expression translates to an external call that returns a string.
3. `CALL NAME(...)` translates to an external call.

#### Option `TYPE`

If `TYPE` is set:

1. Variables, numbers and functions (if `FULLDEF` or `EXTERN` is set) can include the following suffixes:
   - `~` to indicate a C `char`.
   - `%` to indicate a C `short`.
   - `|` to indicate a C `int`.
   - `&` to indicate a C `long`.
   - `@` to indicate a C `void*` (i.e. an unsigned integer type with the same width as a pointer).
   - `!` to indicate a C `float` (or the smallest floating-point type natively supported).
2. The `CAST` function casts its argument to the function type (e.g. `CAST~(123&)` casts a `long` to a `char`).

Many functions promote their arguments to the greatest type, rendering certain casts as no-ops.
For example, `PRINT CAST~(500), 500~` will print `500` twice, even though 500 would not fit in a byte.

#### Option `PTR`

If `PTR` is set:

1. `PTR(Var)` returns a pointer to a variable.
2. `DEREF(Pointer)` dereferences a pointer. The type of the function determines the type of the cast.

#### Linking

External functions have the following naming scheme in the generated assembly code:

- The name of the function (without type suffixes).
- If type is string, `$`.
- If type is enabled by `TYPE`, add a type character after `$`.

If arguments are provided:
- Two underscores (`__`).
- For every argument:
  - If argument is (double) floating-point, `d`.
  - If argument is string, `S`.
  - If `TYPE` is set:
    - If argument is (single) floating-point, `f`.
    - If argument is byte, `c`.
    - If argument is short, `s`.
    - If argument is integer, `i`.
    - If argument is long, `l`.
    - If argument is a pointer, `p`.

## Tests

Test files were mostly taken from bas55.

## Licence

This work is licenced under the EUPL-1.2.