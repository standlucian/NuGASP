# Build & Git Workflow Rules

- Automatically rebuild the application binary (`make`) whenever source code changes are made.
- Do not rebuild the AppImage (`build_appimage.sh`), run `git commit`, or run `git push` unless the user explicitly asks you to.
- Check in periodically and remind the user to rebuild the AppImage, commit, and push when there are unpackaged or unpushed changes—especially before wrapping up when the user says goodbye.
- Whenever the user explicitly asks to make a `git commit`, always run `git push` immediately after the commit succeeds so local changes stay synced to the remote repository.
