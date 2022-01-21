
# M68EMU ![Badge C] ![Badge Python]
*A [`Motorola 68HC05`][CPU] CPU Core Emulator.*

This is a noddy little cycle-counting emulation of the `68HC05` CPU core. <br>
It exists because I couldn't find a project that fitted my needs  !

---

<br>

## Features

- `68HC05` Core Emulation .

    + *Without* ***Peripherals***
    + *With* ***Cycle Counting***


- Hook based **Memory Access** .

- Opcodes with separate **Fetch Hooks** .

    *To handle CPU cores with scrambled opcodes.*

<br>

---

<br>

## Opcodes

### Requirements

- **[Python]**

    ```sh
    sudo apt install python3
    ```

<br>

### Building

Create the **[`opcodes_m68hc05.csv`][Opcodes Table]** file with :

- **[`make_empty_optable.py`][Opcodes Empty]** for empty dummy instructions.

- **[`makeoptab.py`][Opcodes Full]** for pre - generated opcodes.

<br>

---

<br>

## Emulator

### Requirements

- **[Make]**

    ```sh
    sudo apt install build-essential
    ```

<br>

### Building

1. **[Download]** or **Clone** the repository.

2. Navigate to the **[`/Source/`][Source]** folder.

3. Build the program with **Make** command.

    ```sh
    make
    ```
    

<!----------------------------------------------------------------------------->

[Opcodes Empty]: Source/optable/make_empty_optable.py
[Opcodes Full]: Source/optable/makeoptab.py
[Opcodes Table]: Source/optable/opcodes_m68hc05.csv

[Source]: Source

[Make]: https://www.gnu.org/software/make/
[Download]: https://github.com/philpem/m68emu/archive/refs/heads/master.zip
[Python]: https://www.python.org/

[CPU]: https://en.wikipedia.org/wiki/Motorola_68HC05

[Badge C]: https://img.shields.io/badge/C-00599C?style=for-the-badge
[Badge Python]: https://img.shields.io/badge/Python-14354C?style=for-the-badge&logo=python&logoColor=white
