# xrdesktop

A library for XR interaction with classical desktop compositors.

## Build

#### Configure the project
```
$ meson build
```

#### Compile the project
```
$ ninja -C build
```

#### Build the docs
```
ninja -C build xrdesktop-doc
```

## Run

#### Run the examples
Run the scene client.
```
$ ./build/examples/client
```

Run the overlay client.
```
$ ./build/examples/client -o
```

#### Run the tests

Run all tests
```
$ ninja -C build test
```

Don't run tests that require a running XR runtime.
```
meson test -C build/ --no-suite xrdesktop:xr
```

Don't run tests that require a running XR runtime or the installed package.
```
meson test -C build/ --no-suite xrdesktop:xr --no-suite xrdesktop:post-install
```

Since meson `0.46` the project name can be omitted from the test suite:
```
meson test -C build/ --no-suite xr --no-suite post-install
```