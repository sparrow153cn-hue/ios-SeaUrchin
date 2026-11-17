#!/usr/bin/env bash
# Run from the repo root BEFORE pushing:   bash scripts/verify_setup.sh
# Checks that the CI-relevant files are in the right place and are the current versions.
cd "$(dirname "$0")/.." || exit 1
bad=0
ok()   { echo "  ok       $1"; }
fail() { echo "  PROBLEM  $1"; bad=1; }
has()  { grep -q -- "$2" "$1" 2>/dev/null; }

echo "Repo root: $(pwd)"
for f in project.yml Seaurchin/PrecompiledHeader.h scripts/fetch_bass.sh scripts/fetch_deps.sh .github/workflows/ios-build.yml; do
  [ -f "$f" ] && ok "exists: $f" || fail "missing: $f"
done

echo "Workflow folder must contain only the workflow:"
extra=$(ls .github/workflows 2>/dev/null | grep -v '^ios-build\.yml$')
[ -z "$extra" ] && ok ".github/workflows has only ios-build.yml" || fail ".github/workflows has extra files: $(echo $extra) (move them, e.g. project.yml belongs in the repo root)"

echo "Current versions:"
has Seaurchin/PrecompiledHeader.h '__cplusplus'                 && ok "PrecompiledHeader.h is C++-guarded"                     || fail "PrecompiledHeader.h is the OLD version"
has project.yml 'SeaurchinThirdPartyC'                          && ok "project.yml has the split third-party targets"          || fail "project.yml is the OLD version (no SeaurchinThirdPartyC)"
has project.yml 'CLANG_ENABLE_MODULES: NO'                      && ok "project.yml disables Clang modules"                     || fail "project.yml is the OLD version (modules not disabled)"
has project.yml 'ThirdParty/bass/bass.xcframework'              && ok "project.yml links BASS as xcframework"                  || fail "project.yml does not link bass.xcframework (it is fine if fetch_bass.sh migrates it in CI)"
has scripts/fetch_bass.sh 'old layout migrated'                 && ok "fetch_bass.sh can migrate an old project.yml"           || fail "fetch_bass.sh is an OLD version"
has .github/workflows/ios-build.yml 'Preflight'                 && ok "workflow has the preflight step"                        || fail "workflow is an OLD version (no Preflight)"
has .github/workflows/ios-build.yml 'Diagnostics'               && ok "workflow has the diagnostics step"                      || fail "workflow is an OLD version (no Diagnostics)"

[ $bad = 0 ] && echo "ALL GOOD - safe to commit and push." || echo "FIX THE PROBLEMS ABOVE FIRST."
exit $bad
