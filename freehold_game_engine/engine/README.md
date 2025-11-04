Freehold Game Engine
====================

Directory structure
-------------------

```
    .
    ├── engine
    │   ├── core                # Platform independent code - compiled into static lib
    │   │   ├── include         # Public headers
    │   │   └── src
    │   ├── platform            # Platform specific non-code files
    │   │   ├── android
    │   │   ├── default
    │   │   ├── ios
    │   │   └── osx
    │   ├── src                 # Platform specific code
    │   │   ├── android
    │   │   ├── default
    │   │   ├── ios
    │   │   ├── ios_sim
    │   │   ├── linux
    │   │   └── windows
    │   ├── test                # Unit tests
    │   └── tools               # Engine tools, e.g. shader compiler
    └── examples                # Example projects
```
