#!/bin/sh
# docker-entrypoint.sh

# Generate configuration files based on environment variables
envsubst <"/app/conf/db.conf.tmpl" >"/app/db.conf"
envsubst <"/app/conf/game.conf.tmpl" >"/app/game.conf"

# Run the standard container command.
exec "$@"
