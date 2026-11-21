# Working on the fork

Use a feature branch and a pull request for changes. Keep `master` as the reviewed integration branch; do not push directly to it. Codex work uses `codex/<topic>` branch names. Keep commits focused and preserve unrelated working-tree changes.

Before requesting review:

1. Run `scripts/build.ps1` to build the editor and run the offline compatibility tests.
2. For runtime changes, test in an isolated Training runner. Check Apply/Reset, palette switching, match transitions, and cleanup. Record which scenarios received visual confirmation.
3. For file-format changes, preserve the legacy `.pal` prefix, test old files and extension failures, and update `docs/PAL-FORMAT.md`.
4. Run `git diff --check` and inspect the staged diff. Keep generated binaries, build output, personal palettes, settings, raw process dumps and local test data out of commits.
5. Open a draft pull request describing the resulting behavior, validation and remaining limits. Merge or publish a release only after review.

`origin` is this fork. `upstream` is the original project; upstream updates should be reviewed on a separate branch. Runtime offsets and native layouts currently target game build 26588330. Build and test output belongs in ignored `build/` and `dist/` directories.
