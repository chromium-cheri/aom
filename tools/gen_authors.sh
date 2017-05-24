#!/bin/bash

# Add organization names manually.

cat <<EOF
# This file is automatically generated from the git commit history
# by tools/gen_authors.sh.

$(git log --pretty=format:"%aN <%aE>" | sort | uniq | grep -v "corp.google\|clang-format")
EOF
