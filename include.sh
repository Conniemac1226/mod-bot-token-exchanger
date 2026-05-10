#!/usr/bin/env bash

MOD_BOT_TOKEN_EXCHANGER_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/" && pwd )"

source "${MOD_BOT_TOKEN_EXCHANGER_ROOT}/conf/conf.sh.dist"

if [ -f "${MOD_BOT_TOKEN_EXCHANGER_ROOT}/conf/conf.sh" ]; then
    source "${MOD_BOT_TOKEN_EXCHANGER_ROOT}/conf/conf.sh"
fi
