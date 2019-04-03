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
```
$ ./build/examples/overlay_management
$ ./build/examples/scene
```

#### Run the tests
```
$ ninja -C build test
```
