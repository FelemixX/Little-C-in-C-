### Compile C interpreter

```
gcc -O -Wall -Wextra -c -o parser.o ./parser.c
```

```
gcc -O -Wall -Wextra -c -o littlec.o ./littlec.c
```

```
gcc -O -Wall -Wextra -c -o lclib.o ./lclib.c
```

```
gcc -O -Wall -Wextra -o littlec parser.o littlec.o lclib.o
```

### Compile interpreter in one line (CMD)

```
gcc -O -Wall -Wextra -c -o parser.o ./parser.c && gcc -O -Wall -Wextra -c -o littlec.o ./littlec.c && gcc -O -Wall -Wextra -c -o lclib.o ./lclib.c && gcc -O -Wall -Wextra -o littlec parser.o littlec.o lclib.o
```

### Compile Test programs

```
littlec.exe test.c
```

```
littlec.exe test2.c
```
