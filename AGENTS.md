# Project Rules & Guidelines

## Codebase Lookup Preference
- Before performing any internet/web search for display, touchscreen, or hardware solutions for this ESP32 Cheap Yellow Display (CYD) project, **always search the local weather station repository first** at `/home/nicholas/git/nicholaswilde/cyd-weather-station/` for working reference code.
- Before performing any internet/web search for display, touchscreen, or hardware solutions for this ESP32 Cheap Yellow Display (CYD) project, **always search the local photo frame repository second** at `/home/nicholas/git/nicholaswilde/cyd-photo-frame/` for working reference code.
- Before performing any internet/web search for display, touchscreen, or hardware solutions for this ESP32 Cheap Yellow Display (CYD) project, **always search the local ESP32 Plane Radar repository third** at `/home/nicholas/git/MatixYo/ESP32-Plane-Radar/` for working reference code.

## Build and Test Commands
- Build firmware: `pio run -e cyd_28r` or `pio run -e cyd_35c`
- Run host-native tests: `pio test -e native`

## RTK Command Guidelines
- **Git Operations**: Prefix `git` commands with `rtk` (e.g., `rtk git status`, `rtk git diff`, `rtk git log`, `rtk git commit`, `rtk git push`).
- **GitHub CLI**: Prefix `gh` commands with `rtk` (e.g., `rtk gh issue list | cat`, `rtk gh pr status | cat`). Always pipe `gh` commands to `cat` to bypass interactive pagers.
- **File & Directory Inspection**: Use `rtk ls`, `rtk tree`, `rtk find`, or `rtk read` when listing or reading files to get token-optimized output.
- **Searching**: Use `rtk grep` or `rtk rg` for line search pattern matching.
- **Build & Test Outputs**: Use `rtk err` or `rtk test` when running build/test commands to filter output to errors/failures only (e.g. `rtk test pio test -e native`).

## What To Do Next
- When asked "what to do next" (or similar), **always check the remote repository issues first** using `gh`:
  ```bash
  rtk gh issue list | cat
  ```

## Issue Creation
- When asked to create an issue, use your best guess to determine if it is a new feature or a bug fix.
- Prefix the issue title with `[feat]: <description>` or `[bug]: <description>`.
- Add the `enhancement` or `bug` label to the issue accordingly using the `--label` flag with the `gh` command.

<!-- CODEGRAPH_START -->
## CodeGraph

In repositories indexed by CodeGraph (a `.codegraph/` directory exists at the repo root), reach for it BEFORE grep/find or reading files when you need to understand or locate code:

- **MCP tool** (when available): `codegraph_explore` answers most code questions in one call — the relevant symbols' verbatim source plus the call paths between them, including dynamic-dispatch hops grep can't follow. Name a file or symbol in the query to read its current line-numbered source. If it's listed but deferred, load it by name via tool search.
- **Shell** (always works): `codegraph explore "<symbol names or question>"` prints the same output.

If there is no `.codegraph/` directory, skip CodeGraph entirely — indexing is the user's decision.
<!-- CODEGRAPH_END -->
