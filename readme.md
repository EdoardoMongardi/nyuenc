# nyuenc

A simple UNIX shell encryption utility in C that applies secure transformation to command-line input and output.

## Repository Name

**nyuenc**

## Short Description

nyuenc is a lightweight tool for encrypting and decrypting command-line arguments or piped input using a symmetric cipher. It supports customizable keys, various cipher modes (ECB, CBC), and integrates seamlessly into shell pipelines.

## Contents

- **nyuenc.c**: Main source file implementing parsing of arguments, key handling, and encryption/decryption routines.
- **cipher_utils.c / cipher_utils.h**: Utility functions for block cipher operations and padding schemes.
- **Makefile**: Build targets for compiling the `nyuenc` executable and cleaning artifacts.
- **examples/**: Sample scripts demonstrating encryption in pipelines and file-based encryption.
- **.gitignore**: Specifies files and directories to be ignored by Git.

## Building

To compile the encryption utility, run:

```sh
make all
```

This produces the `nyuenc` executable.

## Usage

Encrypt a message from arguments:

```sh
./nyuenc -k mysecretkey -m "Hello World"
```

Decrypt:

```sh
./nyuenc -k mysecretkey -d -m "<ciphertext>"
```

Encrypt data from stdin and output to stdout (CBC mode, random IV):

```sh
echo "Sensitive data" | ./nyuenc -k mysecretkey -mode CBC
```

Supported options:

- `-k, --key <key>`: Symmetric key (required).
- `-m, --message <text>`: Message to encrypt/decrypt.
- `-d, --decrypt`: Decrypt mode (default is encrypt).
- `-mode <ECB|CBC>`: Cipher mode (default CBC).
- `-iv <hex>`: Initialization vector for CBC mode (if not provided, a random IV is generated).

## Examples

See `examples/` for scripts showing how to integrate `nyuenc` with shell pipelines and file operations.

## Cleaning

Remove generated files:

```sh
make clean
```

## License

MIT License. See [LICENSE](LICENSE) for details.

