# Instructions for AI agents

Read [CONTRIBUTING.md](CONTRIBUTING.md) first; it is the source of truth for structure, conventions and tests. The rules below are mandatory and summarize what agents most often get wrong.

## Commits

- Commit after every meaningful unit of work, so history stays traceable. Group only related files per commit.
- Commit messages: **English**, Conventional Commits, imperative mood, subject ≤ 72 chars, written the way a professional human developer would.
- **Never** add `Co-Authored-By: Claude …` or any other AI attribution trailer to commits or pull requests.
- Each commit should build and pass `ctest`.

## Code and docs

- Keep the module dependency rule and folder layout described in CONTRIBUTING.md. New modules come with tests and docs.
- QML uses `Theme`/`Icons` tokens only (Console × Studio design, see `savevault-docs/design/CONSOLE-STUDIO.md`).
- Document professionally for both humans and agents: public headers get a header comment, behavior changes update README, architectural decisions become ADRs.

## Language

- Code, identifiers, commits, logs: English.
- UI text and user-facing docs: Brazilian Portuguese (pt-BR). Talk to the maintainer in pt-BR.

## Verify before committing

```bash
cmake --build --preset debug && ctest --preset debug && cmake --build --preset debug --target all_qmllint
```
