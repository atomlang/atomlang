## Example on how to integrate atomlang VM with in your application.

- Including this example this repository contains several examples on how to integrate
atomlang VM with your application
  - These examples (currently 2 examples)
  - The `cli/` application
  - The `docs/try/main.c` web assembly version of atomlang


- To integrate atomlang VM in you project:
  - Just drag and drop the `src/` directory in your project
  - Add all C files in the directory with your source files
  - Add `src/include` to the include path
  - Compile

----

#### `example1.c` - Contains how to pass values between atomlang VM and C
```
gcc example1.c -o example1 ../../src/*.c -I../../src/include -lm
```

#### `example2.c` - Contains how to create your own custom native type in C
```
gcc example2.c -o example2 ../../src/*.c -I../../src/include -lm
```

