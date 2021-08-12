#!/usr/bin/env bats

@test "stdin: timestamp, no label" {
    run tscat <<<$PATH
    cat << EOF
--- output
$output
--- output
EOF
    match="^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}[+-][0-9]{4} /"

    [ "$status" -eq 0 ]
    [[ "$output" =~ $match ]]
}

@test "stdin: timestamp, label" {
    run tscat test <<<$PATH
    cat << EOF
--- output
$output
--- output
EOF
    match="^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}[+-][0-9]{4} test /"

    [ "$status" -eq 0 ]
    [[ "$output" =~ $match ]]
}

@test "stdin: no timestamp, label" {
    run tscat --format="" test <<<$PATH
    cat << EOF
--- output
$output
--- output
EOF
    match="^test /"

    [ "$status" -eq 0 ]
    [[ "$output" =~ $match ]]
}

@test "stdin: no timestamp, no label" {
    run tscat --format="" <<<$PATH
    cat << EOF
--- output
$output
--- output
EOF
    match="^/"

    [ "$status" -eq 0 ]
    [[ "$output" =~ $match ]]
}

@test "stdin: formatted timestamp" {
    run tscat --format="@%s" <<<$PATH
    cat << EOF
--- output
$output
--- output
EOF
    match="^@[0-9]+"

    [ "$status" -eq 0 ]
    [[ "$output" =~ $match ]]
}

@test "stdin: no output" {
    run tscat --output="0" <<<$PATH
    cat << EOF
--- output
$output
--- output
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ ^$ ]]
}

@test "process restriction: ioctl: redirect stdout to a device" {
    case "$(uname -s)" in
    Linux)
    run script -e -a /dev/null -c "tscat <<<$PATH >/dev/null"
    cat << EOF
--- output
$output
--- output
EOF
    [ "$status" -eq 0 ]
    ;;
    *) skip ;;
    esac
}
