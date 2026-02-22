# NetProj

Client-server system for distributed numerical integration of `1/ln(x)`.

## Build

Requirements:

- Qt 6.5+ (Core, Network, Concurrent)
- CMake 3.19+
- C++17

CMake (generic):

```bash
cmake -S . -B build
cmake --build build
```

## Run

### Server

Run `net_server`.

Tip (Windows): open `cmd` in the folder with executables and use `--pause` to keep console window open.

Example:

```bat
net_server.exe --pause
```

Then enter:

- port
- expected client count `N`
- `A B h method`
  - method: `1` = midpoint rectangles, `2` = trapezoids, `3` = Simpson

### Client

Run `net_client`.

Example (no interactive input):

```bat
net_client.exe --host 127.0.0.1 --port 7777 --pause
```

Interactive mode (fallback) will ask for:

- server host
- server port

The client sends CPU core count (Qt `idealThreadCount()`), receives its interval, computes the partial integral in parallel, then sends result to the server.

## Notes

- Interval must not contain `x = 1` due to singularity of `1/ln(x)`.

## Expected value for quick check

For `A=2`, `B=10`:

`∫[2..10] 1/ln(x) dx ≈ 5.120435`

## Repository hygiene

Do not commit build artifacts (`build/`, `*.exe`, etc.). The provided `.gitignore` already ignores common generated files.
