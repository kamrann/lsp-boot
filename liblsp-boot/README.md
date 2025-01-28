# liblsp-boot - A C++ library

The `liblsp-boot` C++ library provides <SUMMARY-OF-FUNCTIONALITY>.


## Usage

To start using `liblsp-boot` in your project, add the following `depends`
value to your `manifest`, adjusting the version constraint as appropriate:

```
depends: liblsp-boot ^<VERSION>
```

Then import the library in your `buildfile`:

```
import libs = liblsp-boot%lib{<TARGET>}
```


## Importable targets

This package provides the following importable targets:

```
lib{<TARGET>}
```

<DESCRIPTION-OF-IMPORTABLE-TARGETS>


## Configuration variables

This package provides the following configuration variables:

```
[bool] config.liblsp_boot.<VARIABLE> ?= false
```

<DESCRIPTION-OF-CONFIG-VARIABLES>
