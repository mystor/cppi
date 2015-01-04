#!/bin/sh

exit_status=0

for example in *.cppl; do
    set +x
    echo "BUILDING: $example"
    example_o=$(basename $example ".cppl").o
    example_out=$(basename $example ".cppl").out

    set -x
    if ../cppl "$example" "$example_o"; then
        if gcc -o "$example_out" "$example_o"; then
            "./$example_out"
        else
            echo "LINKING OF $example FAILED" >&2
            ((exit_status++))
        fi
    else
        echo "BUILD OF $example FAILED" >&2
        ((exit_status++))
    fi
    set +x
done

exit $exit_status
