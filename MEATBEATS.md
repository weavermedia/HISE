# HISE Meat Beats

## Regular workflow

1. Do my own work in the `meatbeats` branch (personal features, additions, bug fixes etc.)
2. Regularly fetch and merge latest HISE updates into `develop` branch
3. Then merge `develop` into `meatbeats` and build a Debug version

This keeps my local `develop` branch clean and in sync with the original repo.

## Contribution workflow

1. Do contrubution work in the `develop` branch
2. Submit pull request to original repository
3. Merge my contribution into `meatbeats` branch
4. Commit `meatbeats` branch to build a Debug version

This makes sure pull requests are based only on the original repo's `develop` branch.

---

### Fetch and merge latest HISE updates from original repository

- $ git fetch upstream
- $ git checkout develop
- $ git merge upstream/develop

### Merge develop into meatbeats and build Debug version

- $ git checkout meatbeats
- $ git merge develop
- $ git push origin meatbeats
