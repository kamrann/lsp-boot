# liblsp-boot - C++ library wrapping the Language Server Protocol

The `liblsp-boot` C++ library provides type wrappers around the constituent elements of LSP messages, along with a framework to simplify the implementation of an LSP server.


## Usage

To start using `liblsp-boot` in your project, add the following `depends`
value to your `manifest`, adjusting the version constraint as appropriate:

```
depends: liblsp-boot ^0.1.4
```

Then import the library in your `buildfile`:

```
import libs = liblsp-boot%lib{lsp-boot}
```


## Configuration variables

This package provides the following configuration variables:

```
[bool] config.liblsp_boot.enable_import_std ?= ($cxx.class == 'msvc')
```

* `enable_import_std` - Toggles use of `import std` when building the library.
